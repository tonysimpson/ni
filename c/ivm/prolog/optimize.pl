:- consult(insns).
:- dynamic psycodump/1, stackpush/2.
:- dynamic frequency/2, subfrequency/3, bestmodes/1, removefreq/2.
:- index(subfrequency(1,1,0)).


%%% interactive usage %%%

clear :-
        retractall(psycodump(_)).

load(DumpDir) :-
        atom_concat('python ../../../py-utils/ivmextract.py ', DumpDir, CmdLine),
        see(pipe(CmdLine)),
        read(Filename),
        seen,
        loaddumpfile(Filename).

loaddumpfile(Filename) :-
        write('loading '), write(Filename), write('...'), nl,
        see(Filename),
        load_rec,
        seen.

measure(MaxLength) :-
        write('measuring code sequence frequencies...'), nl,
        tell(pipe('python samelines.py "frequency(\'%s\', %d)." > optimize.tmp')),
        (
            L1 = [_,_|_],
            codeslice(L1, MaxLength),
            generalize(L1, L),
            write(L), nl,
            fail
        ;
        told,
        write('measuring subsequence frequencies...'), nl,
        tell(pipe('python samelines.py "subfrequency(\'%s\', %d)." >> optimize.tmp')),
        (
            L1 = [_,_|_],
            gcodesubslice(SubG, G, MaxLength),
            write(SubG), write('\', \''), write(G), nl,
            fail
        ;
        told,
        retractall(frequency(_,_)),
        retractall(subfrequency(_,_,_)),
        loadmeasures)).

loadmeasures :-
        (recorded(allranks, _, Ref) -> erase(Ref); true),
        see('optimize.tmp'),
        load_rec,
        seen.

buildcosts(HighestOpcode) :-
        initial_stack(InitialStack),
        countsuccesses(insn_single_mode(_, _, InitialStack), NbBaseOpcodes),
        MaxLen is HighestOpcode - NbBaseOpcodes,
        retractall(bestmodes(_)),
        retractall(removefreq(_,_)),
        allranks(AllRanks),
        workcostsrec(MaxLen, AllRanks).

show :-
        bestmodes(X),
        write(X), nl,
        fail ;
        true.

emitmodes(HighestOpcode) :-
        buildcosts(HighestOpcode),
        emitmodes.

emitmodes :-
        initial_stack(InitialStack),
        countsuccesses(insn_single_mode(_, _, InitialStack), NbBaseOpcodes),
        FirstOpcode is NbBaseOpcodes+1,
        tell('mode_combine.pl'),
        (
            enumerate(bestmodes(Mode), Opcode, FirstOpcode),
            atom_to_term(Mode, RealMode),
            write(mode_combine(RealMode)),
            write('.  % '),
            write(Opcode),
            nl,
            fail
        ;
        told).


%%% end interactive usage %%%


%preprocess(psycodump(L1), psycodump(L2)) :-
%        joinlist(basemodes, L1, L2).

:- det(atom_to_term/2).
atom_to_term(Atom, Term) :-
        atom_to_term(Atom, Term, []).

load_rec :-
        read(Term),
        Term \= end_of_file,
        !,
        %preprocess(Term, Term1),
        assert(Term),
        load_rec.
load_rec.

codeslice(L1, MaxLength) :-
        psycodump(L),
        subslice(L, L1, MaxLength).

subslice(L, L1, MaxLength) :-
        subslice1(L, L1, MaxLength).
subslice([_|Tail], L1, MaxLength) :-
        subslice(Tail, L1, MaxLength).

subslice1(_, [], _).
subslice1([X|Xs], [X|Ys], MaxLength) :-
        MaxLength > 0,
        N is MaxLength-1,
        subslice1(Xs, Ys, N).

gcodesubslice(SubG, G, MaxLength) :-
        psycodump(L),
        findall(Gs, bagof(G1, L0^L1^L2^(between(2, MaxLength, Len),
                                        length(L1, Len),
                                        append(L0, L1, L2, L),
                                        generalize(L1, G1)),
                          Gs),
                ByLength),
        % ByLength = [[2-slice, ...], [3-slice, ...], ... [MaxLength-slice, ...]]
        write(ByLength), nl,
        triangle_lt(ByLength, SubG, G).

triangle_lt([BaseLine | ExtraLines], A, B) :-
        nth0(I, BaseLine, A),
        I1 is I-1,
        triangle_cut(ExtraLines, I1, I, B).
triangle_lt([_ | ExtraLines], A, B) :-
        triangle_lt(ExtraLines, A, B).

triangle_cut([Line1 | _], Min, Max, B) :-
        triangle_cutline(Line1, Min, Max, 0, B).
triangle_cut([_ | Lines], Min, Max, B) :-
        Min1 is Min-1,
        triangle_cut(Lines, Min1, Max, B).

triangle_cutline([Head|Tail], Min, Max, N, B) :-
        N =< Max,
        (
            (Min =< N, Head = B)
        ;
            N1 is N+1,
            triangle_cutline(Tail, Min, Max, N1, B)).


generalize(L, G) :-
        initial_stack(InitialStack),
        chainlist(generalize1, L, G, ([], InitialStack), _).

:- det(generalize1/4).
generalize1(Term, Mode, (OldOptions, OldStack), (Options, Stack1)) :-
        (memberchk(stack(StackOp), OldOptions) ->
            insn_stack(StackOp, OldStack, Stack1, dummy) ;
            Stack1 = OldStack
        ),
        Term =.. [Insn | Args],
        insn(Insn, FormalArgs, _, Options),
        maplist(generalize_arg(Stack1), FormalArgs, Args, ArgModes),
        Mode =.. [Insn | ArgModes].

%:- det(generalize_arg/4).
generalize_arg(Stack, FormalArg, RealArg, ArgMode) :-
        standard_mode(_, Stack, FormalArg, ArgMode),
        condition_test(ArgMode, RealArg),
        !.

%:- det(typicalexample/2).
%typicalexample(Term1, Term2) :-
%        Term1 =.. [Insn | Args1],
%        maplist(typicalexample_arg, Args1, Args2),
%        Term2 =.. [Insn | Args2].
%
%typicalexample_arg(char, 100).
%typicalexample_arg(int, 1000000).
%typicalexample_arg(indirect(code_t), 100).
%typicalexample_arg(indirect(word_t), 1000000).
%typicalexample_arg(_:B, B).
%typicalexample_arg(N, N) :- integer(N).


complexity(Term, P, Q) :-
        (var(Term) -> Subterms = [] ; Term =.. [_ | Subterms]),
        S is P+1,
        chainlist(complexity, Subterms, S, Q).

modecost(Mode, Cost) :-
        mode_operate(Mode, Code),
        codecost(Code, Cost1),
        Cost is Cost1+1.

moderank(Mode, Freq, Rank) :-
        atom_to_term(Mode, RealMode),
        modecost(RealMode, Cost),
        Rank is Freq/Cost.

% highestrank(+InputRanks, +CurrentBest, -Best)
highestrank([], Best, Best).
highestrank([R1 | Tail], R2, R3) :-
        R1 = rank(Mode, Freq1, Rank1),
        R2 = rank(_, _, Rank2),
        Rank1 > Rank2,
        (removefreq(Mode, Freq1r) ->
            \+ bestmodes(Mode),
            Freq1m is Freq1 - Freq1r,
            moderank(Mode, Freq1m, Rank1m),
            %(Rank1m > Rank2 -> true ;
            %    write('   '),
            %    write(ignored(Mode, Freq1m, Rank1m)),
            %    nl,
            %    fail
            %)
            Rank1m > Rank2
        ;
        true),
        !,
        highestrank(Tail, R1, R3).
highestrank([_ | Tail], R2, R3) :-
        highestrank(Tail, R2, R3).
%highestrank(fork(P1,P2), CurrentBest, Best) :-
%        highestrank(P1, CurrentBest, MiddleBest),
%        highestrank(P2, MiddleBest, Best).

%:- det(correctfrequency/4).
%correctfrequency(SubtractFreq, Insns, InputRanks, [New | Ranks]) :-
%        generalize(Insns, SimilarMode),
%        Current = rank(SimilarMode, MFreq, _),
%        (forkselect(Current, InputRanks, Ranks), ! ;
%         forkselect(selected(Current), InputRanks, Ranks),
%         write('  unselecting')),
%        New = rank(SimilarMode, PatchedFreq, NewRank),
%        PatchedFreq is MFreq - SubtractFreq,
%        moderank(SimilarMode, PatchedFreq, NewRank),
%        write('  patching '),
%        write(Current),
%        write(' -> '),
%        write(New),
%        nl.
%
%forkselect(Elem, List, Rest) :-
%        forkselect1(Elem, List, Rest), !.
%
%forkselect1(Elem, [Elem|Rest], Rest) :- !.
%forkselect1(Elem, [X|Tail], Rest) :- !,
%        forkselect(Elem, Tail, Rest1),
%        Rest = [X|Rest1].
%forkselect1(Elem, fork(P1,P2), Rest) :-
%        (forkselect1(Elem, P1, R1) ->
%            R2 = P2
%        ;
%            forkselect1(Elem, P2, R2),
%            R1 = P1
%        ),
%        Rest = fork(R1,R2).
%
%:- det(forklist/2).
%forklist(List, Tree) :-
%        forklist1(List, [Tree]).
%
%forklist1([H1, H2, H3, H4 | Tail], [fork([H1, H2 | P1],
%                                         [H3, H4 | P2]) | OtherVars]) :-
%        !,
%        append(OtherVars, [P1,P2], Vars),
%        forklist1(Tail, Vars).
%forklist1(List, [List | OtherVars]) :-
%        forklist1([], OtherVars).
%forklist1([], []).

initialsection(SubMode, Mode) :-
        atom_to_term(Mode, RealMode),
        RealSubMode = [_, _ | _],
        append(RealSubMode, [_|_], RealMode),
        term_to_atom(RealSubMode, SubMode).

:- det(buildnextbest/2).
buildnextbest(AllRanks, Exhausted) :-
        highestrank(AllRanks, rank(_, _, -1), rank(FullBestMode, _, _)),
        (var(FullBestMode) ->
            Exhausted = true
        ;
        Exhausted = false,
        (
            (initialsection(BestMode, FullBestMode) ; BestMode = FullBestMode),
            \+ bestmodes(BestMode),
            selectnextbest(AllRanks, BestMode),
            fail
        ;
        true)).

:- det(selectnextbest/2).
selectnextbest(AllRanks, BestMode) :-
        CurrentBest = rank(BestMode, BestFreq, BestRank),
        memberchk(CurrentBest, AllRanks),
        write('selecting '),
        write(CurrentBest),
        nl,
        assertz(bestmodes(BestMode)),
        (removefreq(BestMode, Freqr) ->
            ResidualFreq is BestFreq-Freqr,
            moderank(BestMode, ResidualFreq, EffectiveRank) ;
            EffectiveRank = BestRank
        ),
        recorda(lasteffectiverank, EffectiveRank),
        % rebuild the frequency tree below BestMode
        assertresidualfreq(BestMode),
        ignore(rebuildfreqtree(BestMode)).

rebuildfreqtree(BestMode) :-
        subfrequency(SubMode, BestMode, _),
        assertresidualfreq(SubMode),
        fail.

%assertfreqtree(Mode) :-
%        \+ dynsubmodes(_, Mode),
%        maplist(typicalexample, Mode, Insns),
%        SubInsns = [_, _ | _],
%        append(_, SubInsns, _, Insns),
%        generalize(SubInsns, SubMode),
%        assert(dynsubmodes(SubMode, Mode)),
%        fail.

:- det(assertresidualfreq/1).
assertresidualfreq(SubMode) :-
        findall(Freq, (subfrequency(SubMode,Mode,Freq),
                       bestmodes(Mode)), SuperFreqs),
        list_max(SuperFreqs, 0, SuperFreq),
        setfreqr(SubMode, SuperFreq).

setfreqr(M, Fr) :-
        retractall(removefreq(M, _)),
        assert(removefreq(M, Fr)).

list_max([H|T], Accum, Max) :-
        H > Accum -> list_max(T, H, Max) ; list_max(T, Accum, Max).
list_max([], Max, Max).


%currentrank(AllRanks, Mode, Rank) :-
%        (residualfreq(Mode, -Freq), ! ;
%            (residualfreq(Mode, Freq), ! ;
%                memberchk(rank(Mode, Freq, _), AllRanks))),
%        moderank(Mode, Freq, Rank).

residualfreq(AllRanks, Mode, Freq) :-
        removefreq(Mode, Freqr),
        memberchk(rank(Mode, TotalFreq, _), AllRanks),
        Freq is TotalFreq-Freqr.

killoldranks(AllRanks) :-
        recorded(lasteffectiverank, LimitRank), !,
        setof(Mode, oldkillable(AllRanks, LimitRank, Mode), DiscardModes),
        nl,
        maplist(oldkill, DiscardModes).

oldkillable(AllRanks, LimitRank, (Mode, Rank, LimitRank)) :-
        bestmodes(Mode),
        residualfreq(AllRanks, Mode, Freq),
        moderank(Mode, Freq, Rank),
        %write(x(LimitRank, Mode, Rank)), nl,
        Rank < LimitRank,
        % cannot kill an initial segment of another mode
        \+ (bestmodes(LongerMode), initialsection(Mode, LongerMode)).

oldkill((Mode, Rank, LimitRank)) :-
        write('      unselecting '),
        write(Mode),
        write('  (rank '), write(Rank<LimitRank), write(')'),
        nl,
        retract(bestmodes(Mode)).

:- det(workcostsrec/2).
workcostsrec(MaxLen, AllRanks) :-
        findall(Mode, bestmodes(Mode), Modes),
        length(Modes, Len),
        write(Len/MaxLen),
        write(': '),
        Len >= MaxLen,
        !,
        repeat,
        \+ killoldranks(AllRanks),
        !,
        workcostpostprocess(MaxLen, AllRanks).
workcostsrec(MaxLen, AllRanks) :-
        buildnextbest(AllRanks, Exhausted),
        (Exhausted = true ->
            write('all instruction sequences have been selected.'),
            nl
        ;
        workcostsrec(MaxLen, AllRanks)).

workcostpostprocess(MaxLen, AllRanks) :-
        findall(Mode, bestmodes(Mode), Modes),
        length(Modes, Len),
        (Len < MaxLen ->
            workcostsrec(MaxLen, AllRanks)
        ;
        (Len > MaxLen ->
            last(LastMode, Modes),
            write('      unselecting '),
            write(LastMode),
            nl,
            retract(bestmodes(LastMode)),
            workcostpostprocess(MaxLen, AllRanks)
        ;
        write('done!'),
        nl)).


allranks(AllRanks) :-
        recorded(allranks, AllRanks), !.
allranks(AllRanks) :-
        write('computing ranks...'), nl,
        findall(R, (R=rank(Mode, Freq, Rank),
                    frequency(Mode, Freq),
                    moderank(Mode, Freq, Rank)), AllRanks),
        %forklist(AllRanks1, AllRanks),
        recorda(allranks, AllRanks).


%length_ex(A, Length) :-
%        var(A) -> Length = 0 ;
%        A = [_|B] -> length_ex(B, L), Length is L+1 ;
%        A = [] -> Length = 0 ;
%        Length = 1.
%
%initialslice(_, []).
%initialslice([X|Xs], [X|Ys]) :-
%        initialslice(Xs, Ys).
%
%regmode(Result, Mode) :-
%        memberchk(mode_combine(Mode), Result).

closelist([], []) :- !.
closelist([X|Xs], [X|Ys]) :- !, closelist(Xs, Ys).
closelist(A, [A]).

%processmodes(Result, LenMax, [(_Cost, Mode) | Tail]) :-
%        length_ex(Result, Len1),
%        Len1 < LenMax,
%        !,
%        InitialMode = [_,_|_],
%        findall(InitialMode, initialslice(Mode, InitialMode), InitialModes),
%        maplist(regmode(Result), InitialModes),
%        processmodes(Result, LenMax, Tail).
%processmodes(Result, _, _) :-
%        closelist(Result, Result).

remotecontrol :-
        read(Term),
        Term \= end_of_file,
        !,
        Term,
        remotecontrol.
remotecontrol.


setup :-
        retractall(stackpush(_,_)),
        (
            count_stackpush(Insn, P),
            assert(stackpush(Insn, P)),
            fail
        ;
        true).

:- setup.

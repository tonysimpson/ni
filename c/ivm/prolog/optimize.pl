:- consult(insns).
:- dynamic psycodump/1, stackpush/2.
:- dynamic frequency/2, bestmodes/1, dynsubmodes/2, residualfreq/2.
:- dynamic rfhash/1.


%%% interactive usage %%%

clear :-
        retractall(psycodump(_)).

load(DumpDir) :-
        atom_concat('python ../../../py-utils/ivmextract.py ', DumpDir, CmdLine),
        see(pipe(CmdLine)),
        read(Filename),
        seen,
        see(Filename),
        load_rec,
        seen.

measure(MaxLength) :-
        tell(pipe('python samelines.py "frequency(%s, %d)." > optimize.tmp')),
        (
            L1 = [_,_|_],
            codeslice(L1, MaxLength),
            generalize(L1, L),
            write(L), nl,
            fail
        ) ;
        told,
        retractall(frequency(_,_)),
        loadmeasures.

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
        retractall(dynsubmodes(_,_)),
        retractall(residualfreq(_,_)),
        retractall(rfhash(_)),
        allranks(AllRanks),
        workcostsrec(MaxLen, AllRanks).


show :-
        bestmodes(X),
        write(X), nl,
        fail ;
        true.

emitmodes :-
        initial_stack(InitialStack),
        countsuccesses(insn_single_mode(_, _, InitialStack), NbBaseOpcodes),
        FirstOpcode is NbBaseOpcodes+1,
        tell('mode_combine.pl'),
        (
            enumerate(bestmodes(Mode), Opcode, FirstOpcode),
            write(mode_combine(Mode)),
            write('.  % '),
            write(Opcode),
            nl,
            fail
        ;
        told).


%%% end interactive usage %%%


%preprocess(psycodump(L1), psycodump(L2)) :-
%        joinlist(basemodes, L1, L2).

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
        subchainable(L, L1, MaxLength).

subchainable(L, L1, MaxLength) :-
        subchainable1(L, L1, MaxLength).
subchainable([_|Tail], L1, MaxLength) :-
        subchainable(Tail, L1, MaxLength).

subchainable1([X|_], [X], _).
subchainable1([X1,X2|Xs], [X1,X2|Ys], MaxLength) :-
        MaxLength > 1,
        X1 =.. [Insn | _],
        chainable(Insn),
        N is MaxLength-1,
        subchainable1([X2|Xs], [X2|Ys], N).

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

:- det(typicalexample/2).
typicalexample(Term1, Term2) :-
        Term1 =.. [Insn | Args1],
        maplist(typicalexample_arg, Args1, Args2),
        Term2 =.. [Insn | Args2].

typicalexample_arg(char, 100).
typicalexample_arg(int, 1000000).
typicalexample_arg(indirect(code_t), 100).
typicalexample_arg(indirect(word_t), 1000000).
typicalexample_arg(_:B, B).
typicalexample_arg(N, N) :- integer(N).


complexity(Term, P, Q) :-
        (var(Term) -> Subterms = [] ; Term =.. [_ | Subterms]),
        S is P+1,
        chainlist(complexity, Subterms, S, Q).

trivial_c_arg(Term) :- var(Term).
trivial_c_arg(Term) :- Term =.. [_].

trivial_c_op(X=Y) :- trivial_c_arg(X), trivial_c_arg(Y).

codecost(block_locals(_, L), Cost) :-
        closelist(L, FlatL),
        countsuccesses((member(X, FlatL), \+trivial_c_op(X)), Cost).

modecost(Mode, Cost) :-
        mode_operate(Mode, Code),
        codecost(Code, Cost1),
        Cost is Cost1+1.

moderank(Mode, Freq, Rank) :-
        modecost(Mode, Cost),
        Rank is Freq/Cost.

% highestrank(+InputRanks, +CurrentBest, -Best)
highestrank([], Best, Best).
highestrank([R1 | Tail], R2, R3) :-
        R1 = rank(Mode, _, Rank1),
        R2 = rank(_, _, Rank2),
        Rank1 > Rank2,
        hash_term(Mode, Hash),
        ((rfhash(Hash), residualfreq(Mode, Freq1m)) ->
            \+ bestmodes(Mode),
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


:- det(buildnextbest/1).
buildnextbest(AllRanks) :-
        highestrank(AllRanks, rank(_, _, -1), rank(FullBestMode, _, _)),
        BestMode = [_, _ | _],
        (
            append(BestMode, _, FullBestMode),
            \+ bestmodes(BestMode),
            selectnextbest(AllRanks, BestMode),
            fail
        ) ;
        true.

:- det(selectnextbest/1).
selectnextbest(AllRanks, BestMode) :-
        CurrentBest = rank(BestMode, _, BestRank),
        memberchk(CurrentBest, AllRanks),
        write('selecting '),
        write(CurrentBest),
        nl,
        assertz(bestmodes(BestMode)),
        (residualfreq(BestMode, ResidualFreq) ->
            moderank(BestMode, ResidualFreq, EffectiveRank) ;
            EffectiveRank = BestRank
        ),
        recorda(lasteffectiverank, EffectiveRank),
        ignore(assertfreqtree(BestMode)),
        % rebuild the frequency tree below BestMode
        setof(SubMode, dynsubmodes(SubMode, BestMode), SubModes),
        maplist(retractresidualfreq, SubModes),
        maplist(assertresidualfreq(AllRanks), SubModes, _).

assertfreqtree(Mode) :-
        \+ dynsubmodes(_, Mode),
        maplist(typicalexample, Mode, Insns),
        SubInsns = [_, _ | _],
        append(_, SubInsns, _, Insns),
        generalize(SubInsns, SubMode),
        assert(dynsubmodes(SubMode, Mode)),
        fail.

:- det(retractresidualfreq/1).
retractresidualfreq(Mode) :-
        retractall(residualfreq(Mode, _)).

:- det(assertresidualfreq/3).
assertresidualfreq(_, SubMode, Freq) :-
        residualfreq(SubMode, Freq), !.
assertresidualfreq(AllRanks, SubMode, ResidualFreq) :-
        findall(Mode, (bestmodes(Mode),
                       dynsubmodes(SubMode,Mode),
                       Mode\=SubMode), SuperModes),
        maplist(assertresidualfreq(AllRanks), SuperModes, SuperFreqs),
        memberchk(rank(SubMode, Freq, _), AllRanks),
        chainlist(int_sub, SuperFreqs, Freq, ResidualFreq),
        assert(residualfreq(SubMode, ResidualFreq)),
        hash_term(SubMode, Hash),
        assert(rfhash(Hash)).

int_sub(B, A, C) :-
        C is A-B.


%currentrank(AllRanks, Mode, Rank) :-
%        (residualfreq(Mode, -Freq), ! ;
%            (residualfreq(Mode, Freq), ! ;
%                memberchk(rank(Mode, Freq, _), AllRanks))),
%        moderank(Mode, Freq, Rank).

killoldranks :-
        recorded(lasteffectiverank, LimitRank), !,
        setof(Mode, oldkillable(LimitRank, Mode), DiscardModes),
        nl,
        maplist(oldkill, DiscardModes).

oldkillable(LimitRank, (Mode, Rank, LimitRank)) :-
        bestmodes(Mode),
        residualfreq(Mode, Freq),
        moderank(Mode, Freq, Rank),
        %write(x(Mode, Rank, LimitRank)), nl,
        Rank < LimitRank,
        % cannot kill an initial segment of another mode
        append(Mode, [_|_], LongerMode),
        \+ bestmodes(LongerMode).

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
        \+ killoldranks,
        !,
        workcostpostprocess(MaxLen, AllRanks).
workcostsrec(MaxLen, AllRanks) :-
        buildnextbest(AllRanks),
        workcostsrec(MaxLen, AllRanks).

workcostpostprocess(MaxLen, AllRanks) :-
        findall(Mode, bestmodes(Mode), Modes),
        length(Modes, Len),
        (Len < MaxLen ->
            workcostsrec(MaxLen, AllRanks)
        ;
        (Len > MaxLen ->
            last(LastMode, Modes),
            retract(bestmodes(LastMode)),
            workcostpostprocess(MaxLen, AllRanks)
        ;
        write('done!'),
        nl)).


allranks(AllRanks) :-
        recorded(allranks, AllRanks), !.
allranks(AllRanks) :-
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


setup :-
        retractall(stackpush(_,_)),
        (
            count_stackpush(Insn, P),
            assert(stackpush(Insn, P)),
            fail
        ) ;
        true.

:- setup.

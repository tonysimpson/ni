:- consult(insns).
:- dynamic psycodump/1, stackpush/2.
:- dynamic frequency/2.


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
        see('optimize.tmp'),
        load_rec,
        seen.

show :-
        findall(F, (F=(Rank,L), frequency(L, Freq), modecost(L, Cost),
                    Rank is Freq/Cost), Results),
        sort(Results, Sorted),
        (
            member(X, Sorted),
            write(X), nl,
            fail
        ) ;
        true.

emitmodes(HighestOpcode) :-
        findall(F, (F=(Rank,L), frequency(L, Freq), modecost(L, Cost),
                    Rank is Cost/Freq), Results),
        sort(Results, Sorted),
        initial_stack(InitialStack),
        countsuccesses(insn_single_mode(_, _, InitialStack), NbBaseOpcodes),
        LenMax is HighestOpcode - NbBaseOpcodes,
        processmodes(Result, LenMax, Sorted),
        FirstOpcode is NbBaseOpcodes+1,
        tell('mode_combine.pl'),
        (
            enumerate(member(Clause, Result), Opcode, FirstOpcode),
            Opcode =< HighestOpcode,
            write(Clause),
            write('.'),
            nl,
            fail
        ) ;
        told.


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

%:- det(generalize1/4).
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


complexity(Term, P, Q) :-
        (var(Term) -> Subterms = [] ; Term =.. [_ | Subterms]),
        S is P+1,
        chainlist(complexity, Subterms, S, Q).

codecost(block_locals(_, L), Cost) :-
        closelist(L, FlatL),
        countsuccesses((member(X, FlatL), \+trivial_c_op(X)), Cost).

modecost(Mode, Cost) :-
        mode_operate(Mode, Code),
        codecost(Code, Cost1),
        (Cost1 == 0 -> Cost = 1 ; Cost = Cost1).

trivial_c_arg(Term) :- var(Term).
trivial_c_arg(Term) :- Term =.. [_].

trivial_c_op(X=Y) :- trivial_c_arg(X), trivial_c_arg(Y).


length_ex(A, Length) :-
        var(A) -> Length = 0 ;
        A = [_|B] -> length_ex(B, L), Length is L+1 ;
        A = [] -> Length = 0 ;
        Length = 1.

initialslice(_, []).
initialslice([X|Xs], [X|Ys]) :-
        initialslice(Xs, Ys).

regmode(Result, Mode) :-
        memberchk(mode_combine(Mode), Result).

closelist([], []) :- !.
closelist([X|Xs], [X|Ys]) :- !, closelist(Xs, Ys).
closelist(A, [A]).

processmodes(Result, LenMax, [(_Cost, Mode) | Tail]) :-
        length_ex(Result, Len1),
        Len1 < LenMax,
        !,
        InitialMode = [_,_|_],
        findall(InitialMode, initialslice(Mode, InitialMode), InitialModes),
        maplist(regmode(Result), InitialModes),
        processmodes(Result, LenMax, Tail).
processmodes(Result, _, _) :-
        closelist(Result, Result).


setup :-
        retractall(stackpush(_,_)),
        (
            count_stackpush(Insn, P),
            assert(stackpush(Insn, P)),
            fail
        ) ;
        true.

:- setup.

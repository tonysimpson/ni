:- consult(insns).
:- dynamic psycodump/1, stackpush/2.
:- dynamic frequency/3.


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
        retractall(frequency(_,_,_)),
        tell(user_error),
        (
            L1 = [_,_|_],
            codeslice(L1, MaxLength),
            generalize(L1, L),
            (retract(frequency(L, Freq, Cost)) -> true ;
                Freq = 0,
                mode_operate(L, Code),
                codecost(Code, Cost),
                write('.')
            ),
            F is Freq+1,
            assert(frequency(L, F, Cost)),
            fail
        ) ;
        nl,
        told.

show :-
        findall(F, (F=(Rank,L), frequency(L, Freq, Cost),
                    Rank is Freq/Cost), Results),
        sort(Results, Sorted),
        (
            member(X, Sorted),
            write(X), nl,
            fail
        ) ;
        true.

emitmodes(HighestOpcode) :-
        findall(F, (F=(Rank,L), frequency(L, Freq, Cost),
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
        initial_stack(Stack),
        chainlist(generalize1, L, G, '='(Stack), _).

:- det(generalize1/4).
generalize1(Term, Mode, LazyStack1, LazyStack2) :-
        call(LazyStack1, Stack1),
        Term =.. [Insn | Args],
        insn(Insn, FormalArgs, _, _),
        maplist(generalize_arg(Stack1), FormalArgs, Args, ArgModes),
        Mode =.. [Insn | ArgModes],
        LazyStack2 = insn_operate_stack(Insn, Stack1).

:- det(generalize_arg/4).
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

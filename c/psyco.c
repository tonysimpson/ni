#include "psyco.h"
#include "codemanager.h"
#include "vcompiler.h"
#include "dispatcher.h"
#include "processor.h"
#include "mergepoints.h"
#include "selective.h"
#include "Python/pycompiler.h"
#include "Objects/ptupleobject.h"


#if defined(CODE_DUMP_FILE) && defined(CODE_DUMP_AT_END_ONLY)
# undef psyco_dump_code_buffers
EXTERNFN void psyco_dump_code_buffers(void);
#endif

 /***************************************************************/
/***   Frame and arguments building                            ***/
 /***************************************************************/

DEFINEVAR PyObject* PyExc_PsycoError;

static void fix_run_time_args(PsycoObject * po, vinfo_array_t* target,
                              vinfo_array_t* source, RunTimeSource* sources)
{
  int i = source->count;
  extra_assert(i == target->count);
  while (i--)
    {
      vinfo_t* a = source->items[i];
      if (a != NULL && a->tmp != NULL)
        {
          vinfo_t* b = a->tmp;
          if (is_runtime(a->source))
            {
              if (target->items[i] == NULL)
                continue;  /* item was removed by psyco_simplify_array() */
              if (sources != NULL)
                sources[po->arguments_count] = a->source;
              po->arguments_count++;
              po->stack_depth += sizeof(long);
              /* arguments get borrowed references */
              b->source = RunTime_NewStack(po->stack_depth, REG_NONE,
                                                false);
            }
          extra_assert(b == target->items[i]);
          a->tmp = NULL;
          if (a->array != NullArray)
            fix_run_time_args(po, b->array, a->array, sources);
        }
    }
}

DEFINEFN
PsycoObject* psyco_build_frame(PyFunctionObject* function,
                               vinfo_array_t* arginfo, int recursion,
                               RunTimeSource** sources)
{
  /* build a "frame" in a PsycoObject according to the given code object. */

  PyCodeObject* co = (PyCodeObject*) PyFunction_GET_CODE(function);
  PyObject* merge_points = psyco_get_merge_points(co);
  PyObject* globals;
  PyObject* defaults;
  PsycoObject* po;
  RunTimeSource* source1;
  vinfo_array_t* arraycopy;
  int extras, i, minargcnt, inputargs, rtcount, ncells, nfrees;

  if (merge_points == Py_None)
    goto unsupported;
  
  globals = PyFunction_GET_GLOBALS(function);
  defaults = PyFunction_GET_DEFAULTS(function);
  ncells = PyTuple_GET_SIZE(co->co_cellvars);
  nfrees = PyTuple_GET_SIZE(co->co_freevars);
  if (co->co_flags & (CO_VARARGS | CO_VARKEYWORDS))
    {
      debug_printf(("psyco: unsupported: functions with * or ** arguments\n"));
      goto unsupported;
    }
  if (ncells != 0 || nfrees != 0)
    {
      debug_printf(("psyco: unsupported: functions with free or cell variables\n"));
      goto unsupported;
    }
  minargcnt = co->co_argcount - (defaults ? PyTuple_GET_SIZE(defaults) : 0);
  inputargs = arginfo->count;
  if (inputargs != co->co_argcount)
    {
      if (inputargs < minargcnt || inputargs > co->co_argcount)
        {
          int n = co->co_argcount < minargcnt ? minargcnt : co->co_argcount;
          PyErr_Format(PyExc_TypeError,
                       "%.200s() takes %s %d %sargument%s (%d given)",
                       PyString_AsString(co->co_name),
                       minargcnt == co->co_argcount ? "exactly" :
                         (inputargs < n ? "at least" : "at most"),
                       n,
                       /*kwcount ? "non-keyword " :*/ "",
                       n == 1 ? "" : "s",
                       inputargs);
          goto error;
        }

      /* fill missing arguments with default values */
      arginfo = array_grow1(arginfo, co->co_argcount);
      for (i=inputargs; i<co->co_argcount; i++)
        /* references borrowed from the code object */
        arginfo->items[i] = vinfo_new(CompileTime_New((long) PyTuple_GET_ITEM
						       (defaults, i-minargcnt)));
    }
  
  extras = co->co_stacksize + co->co_nlocals + ncells + nfrees;

  po = PsycoObject_New(INDEX_LOC_LOCALS_PLUS + extras);
  po->stack_depth = INITIAL_STACK_DEPTH;
  po->vlocals.count = INDEX_LOC_LOCALS_PLUS + extras;
  po->last_used_reg = REG_LOOP_START;
  po->pr.auto_recursion = recursion;

  /* duplicate the array of arguments. If two arguments share some common
     part, they will also share it in the copy. */
  if (arginfo->count == 0)
    arraycopy = NullArray;
  else
    {
      clear_tmp_marks(arginfo);
      arraycopy = array_new(arginfo->count);
      duplicate_array(arraycopy, arginfo);
    }

  /* simplify arraycopy in the sense of psyco_simplify_array() */
  rtcount = psyco_simplify_array(arraycopy);

  /* all run-time arguments or argument parts must be corrected: in arginfo
     they have arbitrary sources, but in the new frame's sources they will
     have to be fetched from the machine stack, where the caller will have
     pushed them. */
  if (sources != NULL)
    {
      source1 = PyCore_MALLOC(rtcount * sizeof(RunTimeSource));
      if (source1 == NULL)
        OUT_OF_MEMORY();
      *sources = source1;
    }
  else
    source1 = NULL;
  fix_run_time_args(po, arraycopy, arginfo, source1);
  array_delete(arginfo, NULL);

  /* initialize po->vlocals */
  LOC_GLOBALS = vinfo_new(CompileTime_NewSk(sk_new((long) globals,
                                                SkFlagFixed | SkFlagPyObj)));
  Py_INCREF(globals); /* reference in LOC_GLOBALS */

  /* copy the arguments */
  for (i=0; i<arraycopy->count; i++)
    LOC_LOCALS_PLUS[i] = arraycopy->items[i];
  array_release(arraycopy);

  /* the rest of locals is uninitialized */
  for (; i<co->co_nlocals; i++)
    LOC_LOCALS_PLUS[i] = psyco_vi_Zero();
  /* the rest of the array is the currently empty stack,
     set to NULL by array_new(). */
  
  /* store the code object */
  po->pr.co = co;
  Py_INCREF(co);  /* XXX never freed */
  pyc_data_build(po, merge_points);
  
  po->stack_depth += sizeof(long);  /* count the CALL return address */
  psyco_assert_coherent(po);
  return po;

 error:
  array_delete(arginfo, NULL);
  return NULL;

 unsupported:
  array_delete(arginfo, NULL);
  return BF_UNSUPPORTED;
}

DEFINEFN
vinfo_t* psyco_call_pyfunc(PsycoObject* po, PyFunctionObject* function,
                           vinfo_t* arg_tuple, int recursion)
{
  CodeBufferObject* codebuf;
  PsycoObject* mypo;
  vinfo_array_t* arginfo;
  Source* sources;
  vinfo_t* result;
  int i;

  int tuple_size = PsycoTuple_Load(arg_tuple);
  if (tuple_size == -1)
    goto fail_to_default;
      /* XXX calling with an unknown-at-compile-time number of arguments
         is not implemented, revert to the default behaviour */

  /* prepare a frame */
  arginfo = array_new(tuple_size);
  for (i=0; i<tuple_size; i++)
    {
      vinfo_t* v = PsycoTuple_GET_ITEM(arg_tuple, i);
      arginfo->items[i] = v;
      vinfo_incref(v);
    }
  mypo = psyco_build_frame(function, arginfo, recursion, &sources);
  if (mypo == BF_UNSUPPORTED)
    goto fail_to_default;
  if (mypo == NULL)
    {
      psyco_virtualize_exception(po);
      return NULL;
    }

  /* compile the function (this frees mypo) */
  codebuf = psyco_compile_code(mypo,
                               psyco_first_merge_point(mypo->pr.merge_points));

  /* get the run-time argument sources and push them on the stack
     and write the actual CALL */
  result = psyco_call_psyco(po, codebuf, sources);
  PyCore_FREE(sources);
  return result;

 fail_to_default:
  return psyco_generic_call(po, PyFunction_Type.tp_call,
                            CfReturnRef|CfPyErrIfNull,
                            "lvl", function, arg_tuple, NULL);
}


 /***************************************************************/
/***   PsycoFunctionObjects                                    ***/
 /***************************************************************/

DEFINEFN
PsycoFunctionObject* psyco_PsycoFunction_New(PyFunctionObject* func, int rec)
{
	PsycoFunctionObject* result = PyObject_GC_New(PsycoFunctionObject,
						      &PsycoFunction_Type);
	if (result != NULL) {
		Py_INCREF(func);
		result->psy_func = func;
		result->psy_recursion = rec;
		PyObject_GC_Track(result);
	}
	return result;
}

static void psycofunction_dealloc(PsycoFunctionObject* self)
{
	PyObject_GC_UnTrack(self);
	Py_DECREF(self->psy_func);
	PyObject_GC_Del(self);
}

static PyObject* psycofunction_repr(PsycoFunctionObject* self)
{
	char buf[100];
	if (self->psy_func->func_name == Py_None)
		sprintf(buf, "<anonymous psyco function at %p>", self);
	else
		sprintf(buf, "<psyco function %s at %p>",
			PyString_AsString(self->psy_func->func_name), self);
	return PyString_FromString(buf);
}

static PyObject* psycofunction_call(PsycoFunctionObject* self,
				    PyObject* arg, PyObject* kw)
{
	PyFunctionObject* function = self->psy_func;
	CodeBufferObject* codebuf;
	PsycoObject* po;
	long* initial_stack;
	PyObject* result;
	vinfo_array_t* arginfo;
	int i;
	
	if (kw != NULL && PyDict_Check(kw) && PyDict_Size(kw) > 0) {
		/* keyword arguments not supported yet */
		goto unsupported;
	}

	/* make an array of run-time values */
	i = PyTuple_GET_SIZE(arg);
	arginfo = array_new(i);
	while (i--) {
		/* arbitrary values for the source */
		arginfo->items[i] = vinfo_new(SOURCE_DUMMY);
	}
	
	/* make a "frame" */
	po = psyco_build_frame(function, arginfo, self->psy_recursion, NULL);
	if (po == BF_UNSUPPORTED)
		goto unsupported;
	if (po == NULL)
		return NULL;
	
	/* compile the function */
	codebuf = psyco_compile_code(po,
                           psyco_first_merge_point(po->pr.merge_points));
	
	/* get the actual arguments */
	assert(codebuf->snapshot.fz_arguments_count == PyTuple_GET_SIZE(arg));
	initial_stack = (long*) (&PyTuple_GET_ITEM(arg, 0));

	/* run! */
	result = psyco_processor_run(codebuf, initial_stack);
	
	Py_DECREF(codebuf);
	psyco_trash_object(NULL);  /* free any trashed object now */

#ifdef CODE_DUMP_FILE
        psyco_dump_code_buffers();
#endif

	if (result==NULL)
		extra_assert(PyErr_Occurred());
	else
		extra_assert(!PyErr_Occurred());
	return result;

   unsupported:
	return PyObject_Call((PyObject*) self->psy_func, arg, kw);
}

static int psycofunction_traverse(PsycoFunctionObject *f,
				  visitproc visit, void *arg)
{
	return visit((PyObject*) f->psy_func, arg);
}

static PyObject *
psy_descr_get(PyObject *self, PyObject *obj, PyObject *type)
{
	/* 'self' is actually a PsycoFunctionObject* */
	if (obj == Py_None)
		obj = NULL;
	return PyMethod_New(self, obj, type);
}

DEFINEVAR
PyTypeObject PsycoFunction_Type = {
	PyObject_HEAD_INIT(NULL)
	0,					/*ob_size*/
	"Psyco_function",			/*tp_name*/
	sizeof(PsycoFunctionObject),		/*tp_basicsize*/
	0,					/*tp_itemsize*/
	/* methods */
	(destructor)psycofunction_dealloc,	/*tp_dealloc*/
	0,					/*tp_print*/
	0,					/*tp_getattr*/
	0,					/*tp_setattr*/
	0,					/*tp_compare*/
	(reprfunc)psycofunction_repr,		/*tp_repr*/
	0,					/*tp_as_number*/
	0,					/*tp_as_sequence*/
	0,					/*tp_as_mapping*/
	0,					/*tp_hash*/
	(ternaryfunc)psycofunction_call,	/*tp_call*/
	0,					/* tp_str */
	0,					/* tp_getattro */
	0,					/* tp_setattro */
	0,					/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,/* tp_flags */
	0,					/* tp_doc */
	(traverseproc)psycofunction_traverse,	/* tp_traverse */
	0,					/* tp_clear */
	0,					/* tp_richcompare */
	0,					/* tp_weaklistoffset */
	0,					/* tp_iter */
	0,					/* tp_iternext */
	0,					/* tp_methods */
	0,					/* tp_members */
	0,					/* tp_getset */
	0,					/* tp_base */
	0,					/* tp_dict */
	psy_descr_get,				/* tp_descr_get */
	0,					/* tp_descr_set */
};


 /***************************************************************/
/***   Implementation of the '_psyco' built-in module          ***/
 /***************************************************************/

/* NB: it would be nice to set _psyco.proxy to the PsycoFunction_Type
   as the following function is nothing more than a constructor,
   but this is incompatible with Python versions <2.2b1
*/
static PyObject* Psyco_proxy(PyObject* self, PyObject* args)
{
	int recursion = 0;
	PyFunctionObject* function;
	
	if (!PyArg_ParseTuple(args, "O!|i",
			      &PyFunction_Type, &function,
			      &recursion))
		return NULL;

	return (PyObject*) psyco_PsycoFunction_New(function, recursion);
}

#ifdef CODE_DUMP_FILE
static void vinfo_array_dump(vinfo_array_t* array, FILE* f, PyObject* d)
{
  int i = array->count;
  fprintf(f, "%d\n", i);
  while (i--)
    {
      vinfo_t* vi = array->items[i];
      PyObject* key = PyInt_FromLong((long)vi);
      assert(key);
      fprintf(f, "%ld\n", (long)vi);
      if (vi != NULL && !PyDict_GetItem(d, key))
        {
          switch (gettime(vi->source)) {
          case CompileTime:
            fprintf(f, "ct %ld %ld\n",
                    CompileTime_Get(vi->source)->refcount1_flags,
                    CompileTime_Get(vi->source)->value);
            break;
          case RunTime:
            fprintf(f, "rt %ld\n", vi->source);
            break;
          case VirtualTime:
            fprintf(f, "vt %p\n", VirtualTime_Get(vi->source));
            break;
          default:
            assert(0);
          }
          PyDict_SetItem(d, key, Py_None);
          vinfo_array_dump(vi->array, f, d);
        }
      Py_DECREF(key);
    }
}
DEFINEFN
void psyco_dump_code_buffers(void)
{
  FILE* f = fopen(CODE_DUMP_FILE, "wb");
  if (f != NULL)
    {
      CodeBufferObject* obj;
      PyObject *exc, *val, *tb;
      void** chain;
#ifdef CODE_DUMP_SYMBOLS
      int i1;
      PyObject* global_addrs = PyDict_New();
      assert(global_addrs);
#endif
      PyErr_Fetch(&exc, &val, &tb);
      debug_printf(("psyco: writing " CODE_DUMP_FILE "\n"));

      /* give the address of an arbitrary symbol from the Python interpreter
         and from the Psyco module */
      fprintf(f, "PyInt_FromLong: %p\n", &PyInt_FromLong);
      fprintf(f, "psyco_dump_code_buffers: %p\n", &psyco_dump_code_buffers);
      
      for (obj=psyco_codebuf_chained_list; obj != NULL; obj=obj->chained_list)
        {
          PyObject* d;
          int nsize = obj->codeend - obj->codeptr;
          PyCodeObject* co = obj->snapshot.fz_pyc_data ?
		  obj->snapshot.fz_pyc_data->co : NULL;
          fprintf(f, "CodeBufferObject %p %d %d '%s' '%s' %d '%s'\n",
                  obj->codeptr, nsize, get_stack_depth(&obj->snapshot),
                  co?PyString_AsString(co->co_filename):"",
                  co?PyString_AsString(co->co_name):"",
                  co?obj->snapshot.fz_pyc_data->next_instr:-1,
                  obj->codemode);
          d = PyDict_New();
          assert(d);
          vinfo_array_dump(obj->snapshot.fz_vlocals, f, d);
          Py_DECREF(d);
          fwrite(obj->codeptr, 1, nsize, f);
#ifdef CODE_DUMP_SYMBOLS
          /* look-up all potential 'void*' pointers appearing in the code */
          for (i1=0; i1+sizeof(void*)<=nsize; i1++)
            {
              PyObject* key = PyInt_FromLong(*(long*)(obj->codeptr+i1));
              assert(key);
              PyDict_SetItem(global_addrs, key, Py_None);
              Py_DECREF(key);
            }
#endif
        }

      for (chain = psyco_codebuf_spec_dict_list; chain; chain=(void**)*chain)
        {
          PyObject* spec_dict = (PyObject*)(chain[-1]);
          int i = 0;
          PyObject *key, *value;
          fprintf(f, "spec_dict %p\n", chain);
          while (PyDict_Next(spec_dict, &i, &key, &value))
            {
              PyObject* repr;
              if (PyInt_Check(key))
                {
#ifdef CODE_DUMP_SYMBOLS
                  PyDict_SetItem(global_addrs, key, Py_None);
#endif
                  repr = (key->ob_type->tp_as_number->nb_hex)(key);
                }
              else
                {
#ifdef CODE_DUMP_SYMBOLS
                  key = PyInt_FromLong((long) key);
                  assert(key);
                  PyDict_SetItem(global_addrs, key, Py_None);
#endif
                  repr = PyObject_Repr(key);
#ifdef CODE_DUMP_SYMBOLS
                  Py_DECREF(key);
#endif
                }
              assert(!PyErr_Occurred());
              assert(PyString_Check(repr));
              assert(CodeBuffer_Check(value));
              fprintf(f, "%p %s\n", ((CodeBufferObject*)value)->codeptr,
                      PyString_AsString(repr));
              Py_DECREF(repr);
            }
          fprintf(f, "\n");
        }
#ifdef CODE_DUMP_SYMBOLS
      {
        int i = 0;
        PyObject *key, *value;
        fprintf(f, "symbol table\n");
        while (PyDict_Next(global_addrs, &i, &key, &value))
          {
            Dl_info info;
            void* ptr = (void*) PyInt_AS_LONG(key);
            if (dladdr(ptr, &info) && ptr == info.dli_saddr)
              fprintf(f, "%p %s\n", ptr, info.dli_sname);
          }
        Py_DECREF(global_addrs);
      }
#endif
      assert(!PyErr_Occurred());
      fclose(f);
      PyErr_Restore(exc, val, tb);
    }
}
#endif

/*****************************************************************/

/* Initialize selective compilation */
static PyObject* Psyco_selective(PyObject* self, PyObject* args)
{
  if (!PyArg_ParseTuple(args, "i", &ticks)) {
    return NULL;
  }

  /* Sanity check argument */
  if (ticks < 0) {
    PyErr_SetString(PyExc_ValueError, "negative ticks");
    return NULL;
  }

  /* Allocate a dict to hold counters and statistics in */
  if (funcs == NULL) {
    funcs = PyDict_New();
    if (funcs == NULL)
      return NULL;
  }

  /* Set Python profile function to our selective compilation function */
  PyEval_SetProfile((Py_tracefunc)do_selective, NULL);
  Py_INCREF(Py_None);

  return Py_None;
}

/*****************************************************************/

static PyMethodDef PsycoMethods[] = {
	{"proxy",	&Psyco_proxy,		METH_VARARGS},
	{"selective",   &Psyco_selective,	METH_VARARGS},
	{NULL,		NULL}        /* Sentinel */
};

/* Initialization */
void init_psyco(void)
{
  PyObject* CPsycoModule;

  PsycoFunction_Type.ob_type = &PyType_Type;
  CodeBuffer_Type.ob_type = &PyType_Type;

  CPsycoModule = Py_InitModule("_psyco", PsycoMethods);
  if (CPsycoModule == NULL)
    return;
  PyExc_PsycoError = PyErr_NewException("psyco.error", NULL, NULL);
  if (PyExc_PsycoError == NULL)
    return;
  if (PyModule_AddObject(CPsycoModule, "error", PyExc_PsycoError))
    return;
  Py_INCREF(&PsycoFunction_Type);
  if (PyModule_AddObject(CPsycoModule, "PsycoFunctionType",
			 (PyObject*) &PsycoFunction_Type))
    return;
#ifdef ALL_CHECKS
  if (PyModule_AddIntConstant(CPsycoModule, "ALL_CHECKS", 1))
    return;
#endif
#ifdef PY_PSYCO_MODULE
  PyPsycoModule = PyImport_ImportModule("psyco");
  if (PyPsycoModule == NULL)
    return;
#endif

  psyco_processor_init();
  psyco_dispatcher_init();
  psyco_compiler_init();
  psyco_pycompiler_init();  /* this one last */
  /*if (PyErr_Occurred())
    return;*/
}

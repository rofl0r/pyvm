/*
 *  Implementation of builtin function objects
 * 
 *  Copyright (c) 2006 Stelios Xanthakis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

// this is not a "container" because name and __doc__ are strings.
// so this is not traversed and you can't find it through everything().
// consider immutable.
class BuiltinCallableBase : __permanent__
{
	const char *const stype = BuiltinFuncType;
	const TypeObj &type = &BuiltinFuncTypeObj;
	REFPTR name;
	const unsigned int vf_flags |= VF_CALLABLE;
   public:
	BuiltinCallableBase (const char*, const char* = 0);
	void nc (__object__*);
	__object__ *getattr (__object__*);
	StringObj *repr ();
	void print ();
};


class FuncWrapperObj_noarg : BuiltinCallableBase
{
	no_arg_func f;
   public:
	FuncWrapperObj_noarg (no_arg_func, const char*);
	void nc (void*, __object__*);
	void call (REFPTR, REFPTR[], int);
};

class FuncWrapperObj_fargc : BuiltinCallableBase
{
	int argcount;
	fixed_arg_func f;
   public:
	FuncWrapperObj_fargc (int, fixed_arg_func, const char*);
	void nc (int, void*, __object__*);
	void call (REFPTR, REFPTR[], int);
};


class FuncWrapperObj_vargc : BuiltinCallableBase
{
	int minarg, maxarg;
	var_arg_func f;
   public:
	FuncWrapperObj_vargc (int, int, var_arg_func, const char*);
	void nc (int, int, void*, __object__*);
	void call (REFPTR, REFPTR[], int);
};

class FuncWrapperObj_iargc : BuiltinCallableBase
{
	int minarg;
	var_arg_func f;
   public:
	FuncWrapperObj_iargc (int, var_arg_func, const char*);
	void nc (int, void*, __object__*);
	void call (REFPTR, REFPTR[], int);
};

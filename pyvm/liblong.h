/*
 *  Long integer numbers
 * 
 *  Copyright (c) 2006 Stelios Xanthakis
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) version 3 of the License.
 */

class Long
{
	int sign, aloc, size;
	unsigned int *D;
   public:
	Long (int);
	Long (Long);
	void __size (int, int=1);

	int cmp (int);
	int cmp (Long);
	bool iszero ();

	~Long ();
   /* namespace */
modular	Long *add (Long, Long);
modular	Long *mul (Long, Long);
modular	Long *div (Long, Long);
modular	Long *mod (Long, Long);
modular	Long *sub (Long, Long);
modular	Long *rsh (Long, int);
modular	Long *lsh (Long, int);
modular	Long *and (Long, Long);
modular	Long *pow (Long, Long, Long);
modular	Long *neg (Long);
};

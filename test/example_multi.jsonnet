local myvar = 1;
{
	["foo" + myvar]: [myvar],
	["bar" + myvar]: {
		["baz" + (myvar+1)]: myvar+1,
	}
}


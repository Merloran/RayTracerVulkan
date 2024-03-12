#pragma once

template<typename Type>
struct Handle
{
	Int32 id;

	static const Handle<Type> sNone;
};
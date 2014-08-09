#include <iostream>

#include "boost\preprocessor\repetition\repeat.hpp"
#include "boost\preprocessor\seq\for_each_i.hpp"
#include "boost\preprocessor\tuple\elem.hpp"

#define FIELDS ((bool,a))((int,x))
#define DEF_VAR(r,data,i,elem) BOOST_PP_TUPLE_ELEM(2,0,elem) BOOST_PP_TUPLE_ELEM(2,1,elem);

#define DEF_FIELD(I)
#define FIELD_

struct S
{
	BOOST_PP_SEQ_FOR_EACH_I(DEF_VAR,,FIELDS)
};

int main()
{
	S s = { true, 5 };
	std::cout << sizeof(S)<< " " << s.x << " " << s.a;
	return 0;
}
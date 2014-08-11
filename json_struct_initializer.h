#pragma once

#include "boost\preprocessor\repetition\repeat.hpp"
#include "boost\preprocessor\seq\for_each_i.hpp"
#include "boost\preprocessor\tuple\elem.hpp"
#include "boost\preprocessor\stringize.hpp"
#include "rapidjson\filestream.h"
#include "rapidjson\document.h"

#include <string>

// example list of typevar tuples:
// #define MY_FIELDS ((bool,x))((int,y))

#define DEF_VAR(r,suffix,i,elem) BOOST_PP_TUPLE_ELEM(2,0,elem) BOOST_PP_TUPLE_ELEM(2,1,elem) suffix

#define DEF_ALL_VARS(tuples_list) \
	BOOST_PP_SEQ_FOR_EACH_I(DEF_VAR, ; , tuples_list)

#include <exception>

//class JsonInitException : public std::runtime_error
//{
//public:
//	JsonInitException(char* str)
//		: runtime_error(str)
//	{}
//	JsonInit
//};

#define EXTRACT_FROM_JSON_OBJ(t,a,obj_name) \
	try{\
	GetJsonValue((a), obj_name[BOOST_PP_STRINGIZE(a)]); \
	}\
	catch (std::exception& re) \
		{ std::cerr << re.what() << std::endl; \
		throw std::runtime_error("error initializing parameter: " BOOST_PP_STRINGIZE(a)); }\
	catch (...) { throw std::runtime_error("Unknown error parsing parameter: " BOOST_PP_STRINGIZE(a)); }

#define EXTRACT_FROM_JSON_OBJ_typevar_tuple(r, obj_name, i, typevar_tuple) \
	EXTRACT_FROM_JSON_OBJ(BOOST_PP_TUPLE_ELEM(2, 0, typevar_tuple)\
	, BOOST_PP_TUPLE_ELEM(2, 1, typevar_tuple)\
	, obj_name)

#define OUTPUT_FIELD(FIELD) \
	<< " " BOOST_PP_STRINGIZE(FIELD) " " \
	<< FIELD

#define OUTPUT_FIELD_typevar_tuple(r,d,i,typevar_tuple) \
	OUTPUT_FIELD(BOOST_PP_TUPLE_ELEM(2, 1, typevar_tuple))

#define OUTPUT_ALL(seq_of_typevar_tuples) \
	BOOST_PP_SEQ_FOR_EACH_I(OUTPUT_FIELD_typevar_tuple, 0\
	, seq_of_typevar_tuples)

#define INITIALIZE_FIELDS_FROM_RAPIDJSON_OBJ(list_of_tuples,obj) \
	BOOST_PP_SEQ_FOR_EACH_I(\
	EXTRACT_FROM_JSON_OBJ_typevar_tuple\
	, obj\
	, list_of_tuples)
namespace rj {

	using namespace rapidjson;

	template <typename T> inline T db_to_lin(T value)
	{
		return std::pow(10.0, value / 10.0);
	}
	template <typename T> inline T lin_to_db(T value)
	{
		return 10.0*std::log10(value);
	}

	template <typename T> class OptionallyLogarithmic
	{
		T value;
	public:
		operator T() { return GetLinear(); }
		OptionallyLogarithmic(T _value) { value = _value; }
		OptionallyLogarithmic() {}
		T GetLinear() { return value; }
		T GetDB() { return 10.0*std::log10(value); }
		void SetLinear(T _value) { value = _value; }
		void SetDB(T _value) { value = db_to_lin(_value); }
		void FromRapidJsonObject(rapidjson::Value &obj);
		OptionallyLogarithmic(rapidjson::Value &obj) { FromRapidJsonObject(obj); }
	};


	void GetJsonValue(bool& param, rj::Value &obj);
	void GetJsonValue(int& param, rj::Value &obj);
	void GetJsonValue(unsigned char & param, rj::Value &obj);
	void GetJsonValue(float& param, rj::Value &obj);
	void GetJsonValue(double& param, rj::Value &obj);
	void GetJsonValue(std::string& param, rj::Value &obj);
	template<typename T> void GetJsonValue(OptionallyLogarithmic<T> &param, rj::Value &obj);
}

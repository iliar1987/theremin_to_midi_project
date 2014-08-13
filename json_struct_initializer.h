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


#include <exception>

#include <iostream>
//class JsonInitException : public std::runtime_error
//{
//public:
//	JsonInitException(char* str)
//		: runtime_error(str)
//	{}
//	JsonInit
//};

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
		T GetLinear() const { return value; }
		T GetDB() const { return 10.0*std::log10(value); }
		void SetLinear(T _value) { value = _value; }
		void SetDB(T _value) { value = db_to_lin(_value); }
		void FromRapidJsonObject(rapidjson::Value &obj);
		OptionallyLogarithmic(rapidjson::Value &obj) { FromRapidJsonObject(obj); }
	};

	template<typename T> std::ostream & operator << (std::ostream &out, const OptionallyLogarithmic<T> &OL)
	{
		return out << "linear:\t" << OL.GetLinear()
			<< "\tlogarithmic:\t" << OL.GetDB() << "db"
			<< std::endl;
	}

	//template<typename T> void GetJsonValue(T& param,rj::Value &obj);
	void GetJsonValue(bool& param, rj::Value &obj);
	void GetJsonValue(int& param, rj::Value &obj);
	void GetJsonValue(unsigned char & param, rj::Value &obj);
	
	inline void GetJsonValue(short &param, rj::Value &obj)
	{
		int temp; GetJsonValue(temp, obj); param = static_cast<short>(temp);
	}

	inline void GetJsonValue(unsigned short &param, rj::Value &obj)
	{
		int temp; GetJsonValue(temp, obj); param = static_cast<unsigned short>(temp);
	}
	void GetJsonValue(float& param, rj::Value &obj);
	void GetJsonValue(double& param, rj::Value &obj);
	void GetJsonValue(std::string& param, rj::Value &obj);
	template<typename R> void GetJsonValue(OptionallyLogarithmic<R> &param, rj::Value &obj);
	std::string GetJsonString(Value &obj);
}

#include "fancy_json_macros.h"

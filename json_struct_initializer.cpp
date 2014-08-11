#include "json_struct_initializer.h"

//namespace rj { using namespace rapidjson; }
//
//using namespace rj;

void rj::GetJsonValue(bool& param, rj::Value &obj)
{
	if (!obj.IsBool())
		throw(std::runtime_error("variable is not bool"));
	param = obj.GetBool();
}

void rj::GetJsonValue(int& param, rj::Value &obj)
{
	if (!obj.IsInt())
		throw(std::runtime_error("variable is not int"));
	param = obj.GetInt();
}

void rj::GetJsonValue(unsigned char & param, rj::Value &obj)
{
	if (!obj.IsInt())
		throw(std::runtime_error("variable is not int"));
	param = obj.GetInt();
}

void rj::GetJsonValue(float& param, rj::Value &obj)
{
	if (obj.IsDouble())
		param = static_cast<float>(obj.GetDouble());
	else if (obj.IsInt())
		param = static_cast<float>(obj.GetInt());
	else
		throw(std::runtime_error("variable is of wrong type"));
}

void rj::GetJsonValue(double& param, rj::Value &obj)
{
	if (obj.IsDouble())
		param = obj.GetDouble();
	else if (obj.IsInt())
		param = static_cast<double>(obj.GetInt());
	else
		throw(std::runtime_error("variable is of wrong type"));
}

template<typename T> void rj::GetJsonValue(rj::OptionallyLogarithmic<T> &param, rj::Value &obj)
{
	param.FromRapidJsonObject(reinterpret_cast<rapidjson::Value&>(obj));
}

void rj::GetJsonValue(std::string& param, rj::Value &obj)
{
	if (!obj.IsString())
	{
		throw std::runtime_error("variable is of wrong type");
	}
	int len = obj.GetStringLength();
	const char *str = obj.GetString();
	param.replace(0,len,str);
}


template<typename T> void rj::OptionallyLogarithmic<T>::FromRapidJsonObject(rj::Value &obj)
{
	if (obj.HasMember("db"))
		SetDB(obj["db"].GetDouble());
	else if (obj.HasMember("linear"))
		SetLinear(obj["linear"].GetDouble());
	else
		throw(std::runtime_error("variable should have 'db' or 'linear' member in it."));
}

typedef rj::OptionallyLogarithmic<float> dummy3;
void dummy_func4()
{
	dummy3 someval;
	rj::GetJsonValue(someval, rapidjson::Value());
}

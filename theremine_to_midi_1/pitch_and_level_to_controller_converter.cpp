
#include <cmath>

#include "midi_sender.h"

#include "pitch_and_level_to_controller_converter.h"

#include <iostream>

#include "rapidjson\filestream.h"
#include "rapidjson\document.h"

#include <exception>

#include <cstring>

//damn templates
//template<typename C>
//void GetJsonValue(pltcc::LimitedType<C> &param, rapidjson::Value &obj)
//{
//	if (obj.IsString())
//		param.operator= (rj::GetJsonString(obj));
//	else if (obj.IsInt())
//		param = static_cast<C>(obj.GetInt());
//	else if (obj.IsDouble())
//		param = obj.GetDouble();
//	else
//		throw std::runtime_error("unknown parameter type");
//}

#define GETLIMITEDTYPE_JSONVALUE_MACRO(TYPE) \
	void GetJsonValue(pltcc::LimitedType<TYPE> &param, rapidjson::Value &obj)\
	{\
		if (obj.IsString())\
			param.operator= (rj::GetJsonString(obj));\
		else if (obj.IsInt())\
			param = static_cast<TYPE>(obj.GetInt()); \
		else if (obj.IsDouble())\
			param = obj.GetDouble();\
		else\
			throw std::runtime_error("unknown parameter type");\
	}

namespace rj
{

	GETLIMITEDTYPE_JSONVALUE_MACRO(short)
		GETLIMITEDTYPE_JSONVALUE_MACRO(BYTE)
}
#include "fancy_json_macros.h"

using namespace std;

using namespace pltcc;

using namespace rj;

void dummyX()
{
	Normalizer<float> n();
	
}

template<typename C> LimitedType<C>& LimitedType<C>::operator = (std::string s)
{
	if (!strcmpi(s.c_str(),"min"))
		value = min;
	else if (!strcmpi(s.c_str(), "max"))
		value = max;
	else
		throw runtime_error("unknown string value");
	return *this;
}

template<typename C>
void GetJsonValue(LimitedType<C> &param, rapidjson::Value &obj)
{
	if (obj.IsString())
		param.operator= (rj::GetJsonString(obj));
	else if (obj.IsInt())
		param = static_cast<C>(obj.GetInt());
	else if (obj.IsDouble())
		param = obj.GetDouble();
	else
		throw std::runtime_error("unknown parameter type");
}

ConvertAndSender<float>* NewFromType(const std::string &conv_type)
{
	if (conv_type == "pitch_bend")
		return new LinearValueToPitchBendConverter();
	else if (conv_type == "controller")
		return new LinearValueToControllerConverter();
	else
		throw runtime_error("unknown converter type");
}

void PitchLevelToMidi::FromRapidJsonObject(rapidjson::Value& obj)
{
	INITIALIZE_FIELDS_FROM_RAPIDJSON_OBJ(PitchLevelToMidi_FIELDS, obj)

	pitch_converter = NewFromType(pitch_converter_type);
	pitch_converter->FromRapidJsonObject(obj["pitch_converter"]);
	level_converter=NewFromType(level_converter_type);
	level_converter->FromRapidJsonObject(obj["level_converter"]);
}


void PitchLevelToMidi::FromJsonFile(char* filename)
{
	FILE* f;
	errno_t err = fopen_s(&f, filename, "r");
	if (err)
	{
		std::cerr << "error opening file " << err << std::endl;
	}
	rj::FileStream fs(f);
	rj::Document doc;
	doc.ParseStream(fs);
	rj::Value &v = doc["pitch_level_to_controller_params"];
	FromRapidJsonObject(v);
	fclose(f);
}

template<typename T>
void pltcc::Normalizer<T>::FromRapidJsonObject(rapidjson::Value &obj)
{
	INITIALIZE_FIELDS_FROM_RAPIDJSON_OBJ(NORMALIZER_FIELDS,obj)
}

template<typename T,typename C>
void ValueConverter <T, C>::FromRapidJsonObject(rapidjson::Value &obj)
{
	Normalizer<T>::FromRapidJsonObject(obj);
	INITIALIZE_FIELDS_FROM_RAPIDJSON_OBJ(ValueConverter_FIELDS, obj)
}

template<typename T> 
void ConvertAndSender<T>::FromRapidJsonObject(rapidjson::Value &obj)
{
	INITIALIZE_FIELDS_FROM_RAPIDJSON_OBJ(ConvertAndSender_FIELDS,obj)
}

void ValueToControllerConverter::FromRapidJsonObject(rapidjson::Value &obj)
{
	ValueConverter<float,BYTE>::FromRapidJsonObject(obj);
	ConvertAndSender::FromRapidJsonObject(obj);
	INITIALIZE_FIELDS_FROM_RAPIDJSON_OBJ(ValueToControllerConverter_FIELDS, obj)
		std::cout << (*this);
}

void ValueToPitchBendConverter::FromRapidJsonObject(rapidjson::Value &obj)
{
	ValueConverter<float, short>::FromRapidJsonObject(obj);
	ConvertAndSender<float>::FromRapidJsonObject(obj);
	/*INITIALIZE_FIELDS_FROM_RAPIDJSON_OBJ(ValueToPitchB,obj)*/
	std::cout << (*this);
}

template<typename T> 
T Normalizer<T>::Normalize(T value)
{
	using namespace std;
	if (cleave_top && value > maxval)
		return 1.0;
	if (cleave_bottom && value < minval)
		return 0.0;
	if (logarithmic)
		return normalize_any(
		log2(value), log2(minval), log2(maxval));
	else
		return normalize_any(
		value, minval, maxval);
}

template<typename T> std::ostream& pltcc::operator << (std::ostream& out, const Normalizer<T>& no)
{
	return out OUTPUT_ALL_OBJ(NORMALIZER_FIELDS,no);
}
//
//template<typename T,typename C> std::ostream& operator << (std::ostream& out, ValueConverter<T,C>& lvtcc)
//{
//	out << (Normalizer<T, C>&)lvtcc;
//	return out OUTPUT_ALL(ValueConverter_FIELDS);
//}

//void PitchLevelToMidi::Stop()
//{
//	midis::midi_err_t err = mos.CloseStream();
//	if (err)
//		std::cerr << "midi error: "
//		<< midis::GetMidiErrorMessage(err) << std::endl;
//}
//
//void PitchLevelToMidi::Start(int midi_device_number)
//{
//	midis::midi_err_t err = mos.OpenStream(midi_device_number);
//	if (err)
//		std::cerr << "midi error: "
//		<< midis::GetMidiErrorMessage(err) << std::endl;
//	else
//		my_midi_device_number = midi_device_number;
//}

//void PitchLevelToMidi::Start(std::string midi_out_device_name)
//{
//	Start(midis::GetOutDeviceId(midi_out_device_name));
//}

void ValueToControllerConverter::ConvertAndSend(midis::MidiOutStream &midi_o_s
	,float value)
{
	BYTE c = Convert(value);
	midis::midi_err_t err = midi_o_s.SendController(controller_number
		, c, channel_number);
	if ( err )
		std::cerr << "sending controller failed " 
		<< err << "\t" << midis::GetMidiErrorMessage(err)
		<< std::endl;
}

void ValueToPitchBendConverter::ConvertAndSend(midis::MidiOutStream &midi_o_s
	, float value)
{
	short c = Convert(value);
	
	midi_o_s.SendPitchBend(c, channel_number);
}

void PitchLevelToMidi::ConvertPitchLevelAndSend(midis::MidiOutStream &mos,float pitch, float level)
{
	//std::cerr << "sending" << std::endl;
	using namespace std;
	/*BYTE c_pitch = pitch_converter->Convert(pitch);
	BYTE c_level = level_converter->Convert(level);
	*/
	if (pitch //pitch may be 0 if detection failed
		&& send_pitch
		//&& level >= level_gate
		)
	{
		/*mos.SendController(pitch_coupled_controller_number
			, c_pitch, channel_number);*/
		pitch_converter->ConvertAndSend(mos, pitch);
	}
	if (send_level)
	{
		/*mos.SendController(level_coupled_controller_number
			, c_level, channel_number);*/
		level_converter->ConvertAndSend(mos, level);
	}
}
//
//void pltcc::InitializeDefaultPitchLevelToMidi(PitchLevelToMidi& p)
//{
//	using namespace std;
//	
//	StandardTypeLinearConverter* pitch_converter =
//		new StandardTypeLinearConverter();
//	pitch_converter->cleave_bottom = true;
//	pitch_converter->cleave_top = true;
//	pitch_converter->logarithmic = true;
//	pitch_converter->maxval = 5000;
//	pitch_converter->minval = 500;
//	pitch_converter->controller_max = 127;
//	pitch_converter->controller_min = 0;
//	p.pitch_converter = shared_ptr<StandardTypeConverter>(pitch_converter);
//
//	StandardTypeLinearConverter* level_converter =
//		new StandardTypeLinearConverter();
//	level_converter->cleave_bottom = true;
//	level_converter->cleave_top = true;
//	level_converter->logarithmic = true;
//	level_converter->maxval = db_to_lin(-1.0);
//	level_converter->minval = db_to_lin(-45.0);
//	level_converter->controller_max = 127;
//	level_converter->controller_min = 0;
//	p.level_converter = shared_ptr<StandardTypeConverter>(level_converter);
//
//	p.send_level = true;
//	p.send_pitch = true;
//
//	//p.level_gate = db_to_lin(-45.0);
//	p.level_coupled_controller_number = 81;
//	p.pitch_coupled_controller_number = 84;
//	p.channel_number = 0;
//
//	p.Start(1);
//}
//
typedef Normalizer<float> dummy1;
typedef LinearValueConverter<float, BYTE> dummy2;

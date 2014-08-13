
#include <cmath>

#include "midi_sender.h"

#include "pitch_and_level_to_controller_converter.h"

#include <iostream>
#include <sstream>
#include "rapidjson\filestream.h"
#include "rapidjson\document.h"

#include <exception>

#include <cstring>

#include "boost/lexical_cast.hpp"

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
	//INITIALIZE_FIELDS_FROM_RAPIDJSON_OBJ(PitchLevelToMidi_FIELDS, obj)
	rj::Value& types = obj["types"];
	if (!types.IsObject())
	{
		throw runtime_error("'types' should be object");
	}
	rj::Value& converter_parameters = obj["converter_parameters"];
	if (!converter_parameters.IsObject())
	{
		throw runtime_error("'converter_parameters' should be object");
	}
	for (rj::Value::MemberIterator it
		= types.MemberBegin()
		; it != types.MemberEnd()
		; it++)
	{
		const char* converter_num_str = it->name.GetString();
		int converter_num = boost::lexical_cast<int>(converter_num_str);
		if (!converter_parameters.HasMember(converter_num_str) )
		{
			std::ostringstream s;
			s << "missing converter params for converter number: "
				<< converter_num;
			throw std::runtime_error(s.str());
		}
		ConverterWithType& this_converter = converters[converter_num];
		const rj::Value& input_type_var = *it->value.Begin();
		const rj::Value& output_type_var = it->value[int(1)];
		const char* name_of_input_type = input_type_var.GetString();
		const char* name_of_output_type = output_type_var.GetString();
		if (!strcmpi(name_of_input_type, "pitch"))
			this_converter.converter_input_type = converter_type_pitch;
		else if (!strcmpi(name_of_input_type, "level"))
			this_converter.converter_input_type = converter_type_level;
		else
		{
			this_converter.converter_input_type = converter_type_invalid;
			std::ostringstream s;
			s << "bad converter input type"
				<< converter_num;
			throw runtime_error(s.str());
		}
		this_converter.converter = NewFromType(name_of_output_type);
		this_converter.converter->FromRapidJsonObject(converter_parameters[converter_num_str]);
	}
}

void PitchLevelToMidi::ToggleByKey(char key)
{
	if (key == 'p' || key == 'l')
	{
		for (ConverterContainer::iterator it = converters.begin()
			; it != converters.end()
			; it++)
		{
			if (( it->second.converter_input_type==converter_type_pitch
				&& key == 'p')
				||
				(it->second.converter_input_type == converter_type_level
				&& key == 'l'))
			{
				it->second.converter->send = !it->second.converter->send;
			}
		}
	}
	else if (key >= '0' && key <= '9')
	{
		int converter_number = key - '0';
		if (converters.find(converter_number)
			== converters.end())
		{
			std::cerr << "bad converter number" << std::endl;
		}
		else
		{
			converters[converter_number].converter->send = !converters[converter_number].converter->send;
		}
	}
	else
	{
		std::cerr << "unknown key pressed" << std::endl;
	}
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
	last_sent_value = c;
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
	last_sent_value = c;
	midi_o_s.SendPitchBend(c, channel_number);
}

std::string ValueToControllerConverter::GetOutputType()
{
	ostringstream s;
	s << "CC" << this->controller_number;
	return s.str();
}

void PitchLevelToMidi::ConvertPitchLevelAndSend(midis::MidiOutStream &mos,float pitch, float level)
{
	using namespace std;
	
	for (ConverterContainer::iterator it
		= converters.begin()
		; it != converters.end()
		; it++)
	{
		if (it->second.converter_input_type == converter_type_pitch)
		{
			if (pitch > 0.0 //pitch may be 0 if detection failed
				&& it->second.converter->send)
			{
				//std::cerr << "sent pitch" << pitch << std::endl;
				it->second.converter->ConvertAndSend(mos, pitch);
			}
		}
		else if (it->second.converter_input_type == converter_type_level)
		{
			if (it->second.converter->send)
				it->second.converter->ConvertAndSend(mos, level);
		}
	}
}

std::ostream& PitchLevelToMidi::OutputLastSentValues(std::ostream &out)
{
	for (ConverterContainer::iterator it
		= converters.begin()
		; it != converters.end()
		; it++)
	{
		ConvertAndSender<float>* thisconverter = it->second.converter;
		char input_type_char;
		switch (it->second.converter_input_type)
		{
		case converter_type_pitch:
			input_type_char = 'p';
			break;
		case converter_type_level:
			input_type_char = 'l';
			break;
		default:
			input_type_char = '?';
			break;
		}
		out << input_type_char
			<< "->"
			<<thisconverter->GetOutputType()
			<< ":"
			<< thisconverter->LastSentValue()
			<< " ";
	}
	return out;
}

typedef Normalizer<float> dummy1;
typedef LinearValueConverter<float, BYTE> dummy2;

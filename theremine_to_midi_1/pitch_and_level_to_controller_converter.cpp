
#include <cmath>

#include "midi_sender.h"

#define PLTCC_H_DONT_CLEANUP 1
#include "pitch_and_level_to_controller_converter.h"

using namespace pltcc;

#include <iostream>

#include "rapidjson\filestream.h"
#include "rapidjson\document.h"
//#include "boost\preprocessor\stringize.hpp"
//#include "boost\preprocessor\control\expr_if.hpp"
//#include "boost\preprocessor\comparison\equal.hpp"
//#include "boost\preprocessor\control\if.hpp"
//#include "boost\preprocessor\control\iif.hpp"

#include <exception>

using namespace std;

//namespace rj
//{
//	using namespace rapidjson;
//}
//
//#define EXTRACT_FROM_JSON_OBJ(t,a,obj_name) \
//	try{\
//	GetJsonValue((a), obj_name[BOOST_PP_STRINGIZE(a)]); \
//	}\
//	catch (exception& re) { std::cerr << re.what() << std::endl; throw runtime_error("error initializing parameter: " BOOST_PP_STRINGIZE(a)); }\
//	catch (...) { throw runtime_error("Unknown error parsing parameter: " BOOST_PP_STRINGIZE(a)); }
//#define EXTRACT_FROM_JSON_OBJ_typevar_tuple(r, obj_name, i, typevar_tuple) \
//	EXTRACT_FROM_JSON_OBJ(BOOST_PP_TUPLE_ELEM(2,0,typevar_tuple)\
//	,BOOST_PP_TUPLE_ELEM(2,1,typevar_tuple)\
//	,obj_name)
//#define OUTPUT_FIELD(FIELD) \
//	<< " " BOOST_PP_STRINGIZE(FIELD) " " \
//	<< FIELD
//#define OUTPUT_FIELD_typevar_tuple(r,d,i,typevar_tuple) \
//	OUTPUT_FIELD(BOOST_PP_TUPLE_ELEM(2,1,typevar_tuple))
//#define OUTPUT_ALL(seq_of_typevar_tuples) \
//	BOOST_PP_SEQ_FOR_EACH_I(OUTPUT_FIELD_typevar_tuple, 0\
//	, seq_of_typevar_tuples)

void PitchLevelToMidi::FromRapidJsonObject(rapidjson::Value& obj)
{
	pitch_converter.reset(new LinearValueToControllerConverter<float, BYTE>());
	pitch_converter->FromRapidJsonObject(obj["pitch_converter"]);
	level_converter.reset(new LinearValueToControllerConverter<float, BYTE>());
	level_converter->FromRapidJsonObject(obj["level_converter"]);
	INITIALIZE_FIELDS_FROM_RAPIDJSON_OBJ(PitchLevelToMidi_FIELDS,obj)
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
	/*BOOST_PP_SEQ_FOR_EACH_I(\
		EXTRACT_FROM_JSON_OBJ_typevar_tuple
		, obj
		, NORMALIZER_FIELDS)*/
	INITIALIZE_FIELDS_FROM_RAPIDJSON_OBJ(NORMALIZER_FIELDS,obj)
}

template<typename T,typename C>
void ValueToControllerConverter <T, C>::FromRapidJsonObject(rapidjson::Value &obj)
{
	Normalizer<T>::FromRapidJsonObject(obj);
	/*BOOST_PP_SEQ_FOR_EACH_I(\
		EXTRACT_FROM_JSON_OBJ_typevar_tuple
		, obj
		, ValueToControllerConverter_FIELDS)*/
	INITIALIZE_FIELDS_FROM_RAPIDJSON_OBJ(ValueToControllerConverter_FIELDS, obj)
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

template<typename T> std::ostream& operator << (std::ostream& out, Normalizer<T>& no)
{
	return out OUTPUT_ALL(NORMALIZER_FIELDS);
}

template<typename T,typename C> std::ostream& operator << (std::ostream& out, LinearValueToControllerConverter<T,C>& lvtcc)
{
	out << (NormalError<T, C>&)lvtcc;
	return out OUTPUT_ALL(ValueToControllerConverter_FIELDS);
}

void PitchLevelToMidi::Stop()
{
	midis::midi_err_t err = mos.CloseStream();
	if (err)
		std::cerr << "midi error: "
		<< midis::GetMidiErrorMessage(err) << std::endl;
}

void PitchLevelToMidi::Start(int midi_device_number)
{
	midis::midi_err_t err = mos.OpenStream(midi_device_number);
	if (err)
		std::cerr << "midi error: "
		<< midis::GetMidiErrorMessage(err) << std::endl;
	else
		my_midi_device_number = midi_device_number;
}

void PitchLevelToMidi::ConvertAndSend(float pitch, float level)
{
	using namespace std;
	BYTE c_pitch = pitch_converter->Convert(pitch);
	BYTE c_level = level_converter->Convert(level);
	
	if (pitch //pitch may be 0 if detection failed
		&& send_pitch
			&& level >= level_gate)
		mos.SendController(pitch_coupled_controller_number
			, c_pitch, channel_number);
	if (send_level)
		mos.SendController(level_coupled_controller_number
			, c_level, channel_number);
}

void pltcc::InitializeDefaultPitchLevelToMidi(PitchLevelToMidi& p)
{
	using namespace std;
	
	StandardTypeLinearConverter* pitch_converter =
		new StandardTypeLinearConverter();
	pitch_converter->cleave_bottom = true;
	pitch_converter->cleave_top = true;
	pitch_converter->logarithmic = true;
	pitch_converter->maxval = 5000;
	pitch_converter->minval = 500;
	pitch_converter->controller_max = 127;
	pitch_converter->controller_min = 0;
	p.pitch_converter = shared_ptr<StandardTypeConverter>(pitch_converter);

	StandardTypeLinearConverter* level_converter =
		new StandardTypeLinearConverter();
	level_converter->cleave_bottom = true;
	level_converter->cleave_top = true;
	level_converter->logarithmic = true;
	level_converter->maxval = db_to_lin(-1.0);
	level_converter->minval = db_to_lin(-45.0);
	level_converter->controller_max = 127;
	level_converter->controller_min = 0;
	p.level_converter = shared_ptr<StandardTypeConverter>(level_converter);

	p.send_level = true;
	p.send_pitch = true;

	p.level_gate = db_to_lin(-45.0);
	p.level_coupled_controller_number = 81;
	p.pitch_coupled_controller_number = 84;
	p.channel_number = 0;

	p.Start(1);
}

typedef Normalizer<float> dummy1;
typedef LinearValueToControllerConverter<float, BYTE> dummy2;

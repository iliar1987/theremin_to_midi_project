
#include <cmath>

#include "midi_sender.h"

#include "pitch_and_level_to_controller_converter.h"

using namespace pltcc;

#include <iostream>

#include "rapidjson\filestream.h"
#include "rapidjson\document.h"

#include <exception>

using namespace std;

void PitchLevelToMidi::FromRapidJsonObject(rapidjson::Value& obj)
{
	pitch_converter.reset(new LinearValueConverter<float, BYTE>());
	pitch_converter->FromRapidJsonObject(obj["pitch_converter"]);
	level_converter.reset(new LinearValueConverter<float, BYTE>());
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
	INITIALIZE_FIELDS_FROM_RAPIDJSON_OBJ(ValueToControllerConverter_FIELDS,obj)
}

void ValueToPitchBendConverter::FromRapidJsonObject(rapidjson::Value &obj)
{
	ValueConverter<float, WORD>::FromRapidJsonObject(obj);
	ConvertAndSender<float>::FromRapidJsonObject(obj);
	/*INITIALIZE_FIELDS_FROM_RAPIDJSON_OBJ(ValueToPitchB,obj)*/
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

template<typename T,typename C> std::ostream& operator << (std::ostream& out, ValueConverter<T,C>& lvtcc)
{
	out << (Normalizer<T, C>&)lvtcc;
	return out OUTPUT_ALL(ValueConverter_FIELDS);
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

void PitchLevelToMidi::Start(std::string midi_out_device_name)
{
	Start(midis::GetOutDeviceId(midi_out_device_name));
}

void ValueToControllerConverter::ConvertAndSend(midis::MidiOutStream &midi_o_s
	,float value)
{
	BYTE c = Convert(value);
	midi_o_s.SendController(controller_number
		, c, channel_number);
}

void ValueToPitchBendConverter::ConvertAndSend(midis::MidiOutStream &midi_o_s
	, float value)
{
	short c = Convert(value);
	
	midi_o_s.SendPitchBend(c, channel_number);
}

void PitchLevelToMidi::ConvertAndSend(float pitch, float level)
{
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

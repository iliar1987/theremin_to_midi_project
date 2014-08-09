#include <iostream>

#include "rapidjson\document.h"
#include "rapidjson\filestream.h"
#include "rapidjson\filereadstream.h"
#include "rapidjson\allocators.h"

namespace rj
{
	using namespace rapidjson;
}

#include <stdio.h>
#include <string>

int main(int argc, char* argv[])
{
	rj::Document doc;
	FILE* f;
	errno_t err = fopen_s(&f,"f:\\theremin_to_midi_project\\theremine_to_midi_1\\theremin_params1.json", "r");
	if (err)
	{
		std::cerr << "error opening file " << err << std::endl;
	}
	rj::FileStream fs(f);
	doc.ParseStream(fs);
	rj::Value& v = doc["pitch_level_to_controller_params"];
	assert(v.IsObject());
	rj::Document doc2;
	std::cout << v.operator[] ("send_pitch") .GetBool() << std::endl;
	//std::cout<<doc2["send_pitch"].IsBool();
	//rj::Value::MemberIterator it = v.MemberBegin();
	//std::cout << it->name.GetString() << " " << bool(it->value.GetBool());

	fclose(f);

	return 0;
}
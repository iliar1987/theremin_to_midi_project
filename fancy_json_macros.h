#define DEF_VAR(r,suffix,i,elem) BOOST_PP_TUPLE_ELEM(2,0,elem) BOOST_PP_TUPLE_ELEM(2,1,elem) suffix

#define DEF_ALL_VARS(tuples_list) \
	BOOST_PP_SEQ_FOR_EACH_I(DEF_VAR, ; , tuples_list)


#define EXTRACT_FROM_JSON_OBJ(t,a,obj_name) \
	try{\
	rj::GetJsonValue((a), obj_name[BOOST_PP_STRINGIZE(a)]); \
	}\
	catch (std::exception& re) \
{ std::cerr << re.what() << std::endl; \
throw std::runtime_error("error initializing parameter: " BOOST_PP_STRINGIZE(a)); }\
catch (...) { throw std::runtime_error("Unknown error parsing parameter: " BOOST_PP_STRINGIZE(a)); }

#define EXTRACT_FROM_JSON_OBJ_typevar_tuple(r, obj_name, i, typevar_tuple) \
	EXTRACT_FROM_JSON_OBJ(BOOST_PP_TUPLE_ELEM(2, 0, typevar_tuple)\
	, BOOST_PP_TUPLE_ELEM(2, 1, typevar_tuple)\
	, obj_name)

#define OUTPUT_FIELD_OBJ(FIELD,OBJ) \
	<< " " BOOST_PP_STRINGIZE(FIELD) " " \
	<< (OBJ).FIELD << std::endl

#define OUTPUT_FIELD(FIELD) \
	<< " " BOOST_PP_STRINGIZE(FIELD) " " \
	<< FIELD << std::endl

#define OUTPUT_FIELD_typevar_tuple(r,d,i,typevar_tuple) \
	OUTPUT_FIELD(BOOST_PP_TUPLE_ELEM(2, 1, typevar_tuple))

#define OUTPUT_FIELD_OBJ_typevar_tuple(r,OBJ,i,typevar_tuple) \
	OUTPUT_FIELD_OBJ(BOOST_PP_TUPLE_ELEM(2, 1, typevar_tuple) \
	,OBJ)

#define OUTPUT_ALL(seq_of_typevar_tuples) \
	BOOST_PP_SEQ_FOR_EACH_I(OUTPUT_FIELD_typevar_tuple, 0\
	, seq_of_typevar_tuples)

#define OUTPUT_ALL_OBJ(seq_of_typevar_tuples,OBJ) \
	BOOST_PP_SEQ_FOR_EACH_I(OUTPUT_FIELD_OBJ_typevar_tuple, OBJ\
	,seq_of_typevar_tuples)

#define INITIALIZE_FIELDS_FROM_RAPIDJSON_OBJ(list_of_tuples,obj) \
	BOOST_PP_SEQ_FOR_EACH_I(\
	EXTRACT_FROM_JSON_OBJ_typevar_tuple\
	, obj\
	, list_of_tuples)


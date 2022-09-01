#include "hao_config.h"
#include <filesystem>
#include <algorithm>
using std::filesystem::path;
using std::count;
using std::cerr;

namespace {
	stringstream ss;

	JsonValue ParserVal();

	JsonValue ParserNum()
	{
		string s;
		while (isdigit(ss.peek()) || ss.peek() == 'e' || ss.peek() == '-' || ss.peek() == '+')
		{
			s.push_back(ss.get());
		}
		if (count(s.begin(), s.end(), '.') || count(s.begin(), s.end(), 'e')) {
			return stof(s);
		}
		else {
			return stoi(s);
		}
	}

	string ParserStr()
	{
		ss.get();
		string s;
		while (ss.peek() != '"')
		{
			s.push_back(ss.get());
		}
		ss.get();
		return s;
	}

	bool ParserBool()
	{
		if (ss.peek() == 'f') {
			ss.get(); ss.get(); ss.get(); ss.get(); ss.get();
			return false;
		}
		else {
			ss.get(); ss.get(); ss.get(); ss.get();
			return true;
		}
	}

	vector<JsonValue> ParserArr()
	{
		vector<JsonValue> result;
		// 吃掉 '['
		ss.get();
		while (ss.peek() != ']')
		{
			result.push_back(ParserVal());
			while (ss.peek() != ']' && (ss.peek() == ' ' || ss.peek() == '\t' || ss.peek() == '\n' || ss.peek() == ','))
			{
				ss.get();
			}
		}
		ss.get();
		return result;
	}

	JsonValue ParserMap() {
		ss.get();
		JsonValue map;
		while (ss.peek() != '}') {
			JsonValue key = ParserVal();
			while (ss.peek() == ' ' || ss.peek() == ':')ss.get();
			JsonValue val = ParserVal();
			map.put(key, val);
			while (ss.peek() != '}' && (ss.peek() == ' ' || ss.peek() == '\t' || ss.peek() == '\n' || ss.peek() == ','))
			{
				ss.get();
			}
		}
		ss.get();
		return map;
	}
	void ParserComment()
	{
		while(ss.peek() != '\n')
		{
			ss.get();
		}
		ss.get();
	}
	JsonValue ParserVal()
	{
		while (ss.peek() != -1) {
			if (ss.peek() == ' ' || ss.peek() == '\n' || ss.peek() == '\t')ss.get();
			else if (ss.peek() == '"') {
				return ParserStr();
			}
			else if (ss.peek() == 'f' || ss.peek() == 't') {
				return ParserBool();
			}
			else if (ss.peek() == '[') {
				return  ParserArr();
			}
			else if (ss.peek() == '{') {
				return ParserMap();
			}
			else if (ss.peek() == '/')
			{
				ParserComment();
			}
			else {
				return ParserNum();
			}
		}
		return 0;
	}

	JsonValue parser(string s)
	{
		ss.clear();
		ss << s;
		return ParserVal();
	}
}


bool Config::Load(std::string conf_filename)
{
	path file_path{ conf_filename };
	if (file_path.extension() == ".hjson")
	{
		std::ifstream file(conf_filename);
		stringstream ss;
		ss << file.rdbuf();
		json_data = parser(ss.str());
		return true;
	}
	return false;	
}

Config& Config::GetInstance()
{
	static Config config;
	return config;
}
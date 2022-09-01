#ifndef _HAO_CONFIG_H_
#define _HAO_CONFIG_H_

#include <variant>
#include <string>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <initializer_list>
#include <string_view>
#include <memory>
#include <map>
#include <sstream>
#include <fstream>

using std::variant;
using std::monostate;
using std::string;
using std::unordered_map;
using std::vector;
using std::ostream;
using std::holds_alternative;
using std::get;
using std::initializer_list;
using std::unique_ptr;
using std::map;
using std::string_view;

class JsonValue;
using JsonKey = variant<size_t, string>;
using JsonObject = map<string, JsonValue>;
using JsonArray = vector<JsonValue>;
using std::stringstream;

class JsonValue
{
public:
	JsonValue() = default;
	JsonValue(JsonValue&& rhs) noexcept
	{
		value_ = move(rhs.value_);
	}
	JsonValue(const JsonValue& lhs)
	{
		value_ = lhs.value_;
	}
	JsonValue& operator=(const JsonValue& other)
	{
		value_ = other.value_;
		return *this;
	}
	~JsonValue() = default;
	JsonValue(const int a)
	{
		value_ = static_cast<int>(a);
	}
	JsonValue(const double a)
	{
		value_ = a;
	}
	JsonValue(const char* str)
	{
		value_ = string(str);
	}
	JsonValue(const string str)
	{
		value_ = str;
	}
	JsonValue(const bool a)
	{
		value_ = a;
	}
	JsonValue(const vector<JsonValue>& val)
	{
		value_ = val;
	}
	JsonValue(initializer_list<JsonValue> val)
	{
		value_ = val;
	}
	operator bool() const
	{
		if(holds_alternative<bool>(value_))
		{
			return get<bool>(value_);
		}
		return false;
	}
	operator int() const
	{
		if (holds_alternative<int>(value_))
		{
			return get<int>(value_);
		}
		return 0;
	}
	operator double() const
	{
		if(holds_alternative<double>(value_))
		{
			return get<double>(value_);
		}
		return 0.0;
	}
	operator string() const
	{
		if (holds_alternative<string>(value_))
		{
			return get<string>(value_);
		}
		return "";
	}
	operator string_view () const
	{
		if(holds_alternative<string>(value_))
		{
			return string_view{get<string>(value_)};
		}
		return "";
	}
	
	template<size_t size>
	JsonValue& operator[](const char(&key)[size])
	{
		static JsonValue err;
		if (holds_alternative<JsonObject>(value_))
		{
			return get<JsonObject>(value_)[key];
		}
		return err;
	}
	JsonValue& operator[](int index)
	{
		static JsonValue err;
		if (holds_alternative<JsonArray>(value_))
		{
			return get<JsonArray>(value_)[index];
		}
		return err;
	}
	size_t size()
	{
		if (holds_alternative<JsonObject>(value_))
		{
			return get<JsonObject>(value_).size();
		}
		else if (holds_alternative<JsonArray>(value_))
		{
			return get<JsonArray>(value_).size();
		}
		else
		{
			return 0;
		}
	}
	void put(const JsonValue& key, const JsonValue& value)
	{
		string key_ = get<string>(key.value_);
		get<JsonObject>(value_)[key_] = value;
	}
	string str()
	{
		stringstream ss;
		ss << (*this);
		return ss.str();
	}
	friend ostream& operator << (ostream& out, const JsonValue& val)
	{
		if (holds_alternative<JsonObject>(val.value_))
		{
			JsonObject object = get<JsonObject>(val.value_);
			out << '{';
			for (auto it = object.begin(); it != object.end(); ++it)
			{
				if (it != object.begin())
				{
					out << ',';
				}
				out << '"' << it->first << '"' << ':' << it->second;
				//out << "解析第二个字段:" << it->second << endl;
			}
			out << '}';
		}
		if (holds_alternative<int>(val.value_))
		{
			out << get<int>(val.value_);
		}
		if (holds_alternative<double>(val.value_))
		{
			out << get<double>(val.value_);
		}
		if (holds_alternative<string>(val.value_))
		{
			out << '"' << get<string>(val.value_) << '"';
		}
		if (holds_alternative<JsonArray>(val.value_))
		{
			JsonArray value_array = get<JsonArray>(val.value_);
			out << '[';
			for (size_t i{ 0 }; i < value_array.size(); i++)
			{
				if (i)
				{
					out << ',';
				}
				out << value_array[i];
			}
			out << ']';
		}

		if (holds_alternative<bool>(val.value_))
		{
			if (get<bool>(val.value_))
			{
				out << "true";
			}
			else
			{
				out << "false";
			}
		}
		return out;
	}
private:
	variant <
		JsonObject,
		bool,
		int,
		double,
		string,
		JsonArray
	> value_;
};

class Config
{
    public:
		template<size_t Size>
        JsonValue& operator[](const char(&key)[Size])
		{
			return json_data[key];
		}
		JsonValue& operator[](int index)
		{
			return json_data[index];
		}
		bool Load(std::string conf_filename);
    public:
        static Config& GetInstance();
        Config(const Config&) = delete;
        Config(Config&&) = delete;
        Config& operator=(const Config&) = delete;
        Config& operator=(Config&&) = delete;

    private:
        Config() = default;
        ~Config() = default;		
		JsonValue json_data;
};


#endif
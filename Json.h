﻿// Copyright (c) 2021 tr-takatsuka
// This software is released under the MIT License, https://github.com/tr-takatsuka/rlib-Json

/*
	JSON パーサ です。
	・JSON 仕様に沿ったデータ構造クラスに、パース(parse)と出力(stringify)機能を付加したものです。
	・ヘッダーファイルのみで動作します。
	・C++11 でビルド可です。boost などの外部ライブラリには依存していません。
	・参照や編集等で例外は発生しない設計です。( at() 関数を除く)
	  ・範囲外の読み込みはデフォルト値が取得され、書き込みの場合は要素を作成します。
	・入出力は std::string（UTF-8）のみ対応です。
	・javascript と違い、数値は実数(double)と整数(std::intmax_t)に区別して処理しています。
	・JSON5 の一部仕様を実装しています。
	  ・コメント付きJSONをパース可能です。(オプションで無効に出来ます)
	・JSON Pointer を実装しています。
	・ストリーム入力のパース処理には非対応です。

	・使い方例
		try {
			const rlib::Json j = rlib::Json::parse(				// JSON 文字列から構築
				u8R"({
					"n" : -123.456e+2,
					"list":[
						32,
						"ABC"
					],
					"b": true,
					"c": null
				})");
			double d0 = j["n"].get<double>();					// -123.456e+2 を取得
			double da = j.at("n").get<double>();				// at() で参照する記述です。（範囲外の場合に例外が発生します）
			double d1 = j["e"].get<double>();					// 0.0 を取得 (存在しない位置を指定したのでデフォルト値が取れる)
			std::intmax_t n1 = j["n"].get<std::intmax_t>();		// -12346 を取得 (double値を四捨五入した整数値が取れます)
			std::string s0 = j["list"][1].get<std::string>();	// "ABC" を取得
			std::string sa = j.at(rlib::Json::Pointer("/list/1")).get<std::string>();	// JSON Pointerで指定する記述です。
			std::string s1 = j["ary"][9].get<std::string>();	// 空文字を取得 (存在しない位置を指定したのでデフォルト値が取れる)
			rlib::Json list = j["list"];						// "list"以下をコピー(複製)
			list[10]["add"] = 123;								// [10]の位置に {"add":123} を 追加 ( 配列[2～9]の位置は null で埋められる)
			bool compare = list == j["list"];					// 比較です。false が返ります。
			std::string json = list.stringify();				// JSON 文字列を取得
			list[10].erase("add");								// [10]の位置の連想配列の要素({"add":123})を削除
			list.erase(9);										// [9]の位置の要素(null)を削除
			rlib::Json& c = list.at(10);						// at() で参照すると範囲外の場合に例外が発生します
		} catch (rlib::Json::ParseException& e) {		// パース 失敗
			std::cerr << e.what() << std::endl;
		} catch (std::out_of_range& e) {				// 範囲外参照
			std::cerr << e.what() << std::endl;
		}
*/

#pragma once

#include <cassert>
#include <cmath>
#include <codecvt>
#include <functional>
#include <map>
#include <regex>
#include <tuple>
#include <vector>

namespace rlib
{

	template <class T = void> class JsonT
	{
	public:
		// version (major, minor, patch)
		static std::tuple<int, int, int> version() {
			return std::make_tuple(1, 0, 1);	// 1.0.1
		}
		enum class Type {
			Null,		// null (デフォルト)
			Bool,		// bool
			Float,		// double
			Int,		// std::intmax_t
			String,		// std::string
			Array,		// 配列
			Map,		// 連想配列(オブジェクト)
		};
		typedef std::vector<JsonT> Array;
		typedef std::map<std::string, JsonT> Map;
	private:
		Type				m_type;
		union {
			bool			m_bool;
			double			m_float;
			std::intmax_t	m_int;
			std::string		m_string;
			Array			m_array;
			Map				m_map;
		};
	private:
		static const JsonT			m_emptyJson;
		static const std::string	m_emptyString;
		static const Array			m_emptyArray;
		static const Map			m_emptyMap;
	public:
		JsonT()
			:m_type(Type::Null)
		{}
		explicit JsonT(std::nullptr_t)
			:m_type(Type::Null)
		{}
		explicit JsonT(bool b)
			:m_type(Type::Bool), m_bool(b)
		{}
		explicit JsonT(double n)
			:m_type(Type::Float), m_float(n)
		{}
		explicit JsonT(int n)
			:m_type(Type::Int), m_int(n)
		{}
		explicit JsonT(std::intmax_t n)
			:m_type(Type::Int), m_int(n)
		{}
		explicit JsonT(const char* p)
			:m_type(Type::String), m_string(p)
		{}
		explicit JsonT(const decltype(m_string)& s)
			:m_type(Type::String), m_string(s)
		{}
		explicit JsonT(decltype(m_string) && s)
			:m_type(Type::String), m_string(std::move(s))
		{}
		explicit JsonT(const Array& s)
			:m_type(Type::Array), m_array(s)
		{}
		explicit JsonT(Array&& s)
			:m_type(Type::Array), m_array(std::move(s))
		{}
		explicit JsonT(const Map& s)
			:m_type(Type::Map), m_map(s)
		{}
		explicit JsonT(Map&& s)
			:m_type(Type::Map), m_map(std::move(s))
		{}
		JsonT(const JsonT& s)
			:m_type(Type::Null)
		{
			*this = s;
		}
		JsonT(JsonT&& s)
			:m_type(s.m_type)
		{
			switch (m_type) {
			case Type::Null:																break;
			case Type::Bool:	m_bool = s.m_bool;											break;
			case Type::Float:	m_float = s.m_float;										break;
			case Type::Int:		m_int = s.m_int;											break;
			case Type::String:	new(&m_string) decltype(m_string)(std::move(s.m_string));	break;
			case Type::Array:	new(&m_array) decltype(m_array)(std::move(s.m_array));		break;
			case Type::Map:		new(&m_map) decltype(m_map)(std::move(s.m_map));			break;
			default:			assert(false);
			}
		}

		~JsonT() {
			clear();
		}

		bool operator==(const JsonT& s)const {
			if (m_type == s.m_type) {
				switch (m_type) {
				case Type::Null:	return true;
				case Type::Bool:	return m_bool == s.m_bool;
				case Type::Float:	return m_float == s.m_float;
				case Type::Int:		return m_int == s.m_int;
				case Type::String:	return m_string == s.m_string;
				case Type::Array:	return m_array == s.m_array;
				case Type::Map:		return m_map == s.m_map;
				}
				assert(false);
			}
			return false;
		}

		bool operator!=(const JsonT& s)const {
			return !(*this == s);
		}

		template <typename S> JsonT& operator=(const S& s) {
			*this = JsonT(s);
			return *this;
		}

		JsonT& operator=(const JsonT& s) {
			if (this != &s) {
				clear();
				switch (s.m_type) {
				case Type::Null:	new(this) JsonT(nullptr);	break;
				case Type::Bool:	new(this) JsonT(s.m_bool);	break;
				case Type::Float:	new(this) JsonT(s.m_float);	break;
				case Type::Int:		new(this) JsonT(s.m_int);	break;
				case Type::String:	new(this) JsonT(s.m_string);	break;
				case Type::Array:	new(this) JsonT(s.m_array);	break;
				case Type::Map:		new(this) JsonT(s.m_map);	break;
				default:		assert(false);
				}
			}
			return *this;
		}
		JsonT& operator=(JsonT&& s) {
			if (this != &s) {
				clear();
				new(this) JsonT(std::move(s));
			}
			return *this;
		}

		void clear() {
			typedef std::string TypeString;
			switch (m_type) {
			case Type::String:	m_string.~TypeString();	break;
			case Type::Array:	m_array.~Array();		break;
			case Type::Map:		m_map.~Map();			break;
			}
			m_type = Type::Null;
		}

		// 連想配列を取得 (typeがMapではない場合、Mapに変更した上で返す。連想配列(map)を直接操作したい場合に使うべし)
		Map& ensureMap() {
			if (m_type != Type::Map) {
				clear();
				m_type = Type::Map;
				new(&m_map) decltype(m_map)();
			}
			return m_map;
		}

		// 連想配列から要素を取得 (存在しないキーを指定されたら空実体を返す。例外発生しない)
		const JsonT& operator[](const std::string& key)const {
			const auto& m = get<Map>();
			const auto i = m.find(key);
			return i != m.end() ? i->second : m_emptyJson;
		}

		// 連想配列から要素(参照)を取得 (取得出来るようキーを追加する)
		JsonT& operator[](const std::string& key) {
			return ensureMap()[key];
		}

		// 連想配列から要素を取得 (存在しないキーを指定されたら throw std::out_of_range)
		const JsonT& at(const std::string& key) const noexcept(false) {
			return const_cast<typename std::remove_const<typename std::remove_pointer<decltype(this)>::type>::type*>(this)->at(key);
		}
		JsonT& at(const std::string& key) noexcept(false) {
			if (m_type != Type::Map) throw std::out_of_range("not map");
			const auto i = m_map.find(key);
			if (i == m_map.end()) throw std::out_of_range("invalid key");
			return i->second;
		}

		// 連想配列から指定のキーを削除 (存在しないキーを指定されたら何もしないでfalseを返す)
		// 戻り値 true:削除した false:対象のキーが存在しなかった
		bool erase(const std::string& key) {
			return m_type == Type::Map ? m_map.erase(key) > 0 : false;
		}

		// 配列を取得 (typeがArrayではない場合、Arrayに変更した上で返す。配列(vector)を直接操作したい場合に使うべし)
		Array& ensureArray() {
			if (m_type != Type::Array) {
				clear();
				m_type = Type::Array;
				new(&m_array) decltype(m_array)();
			}
			return m_array;
		}

		// 配列から要素を取得 (範囲外指定は空実体を返す。例外発生しない)
		const JsonT& operator[](size_t index)const {
			const auto& a = get<Array>();
			return index < a.size() ? a[index] : m_emptyJson;
		}

		// 配列から要素(参照)を取得 (取得出来るよう必要に応じて配列を拡張する)
		JsonT& operator[](size_t index) {
			auto& a = ensureArray();
			if (index >= a.size()) a.resize(index + 1);		// 足りないなら作る
			return a[index];
		}

		// 配列から要素を取得 (範囲外指定は throw std::out_of_range)
		const JsonT& at(size_t index) const noexcept(false) {
			return const_cast<typename std::remove_const<typename std::remove_pointer<decltype(this)>::type>::type*>(this)->at(index);
		}
		JsonT& at(size_t index) noexcept(false) {
			if (m_type != Type::Array) throw std::out_of_range("not array");
			if (index >= m_array.size()) throw std::out_of_range("invalid index");
			return m_array[index];
		}

		// 配列から要素を削除 (存在しないindexを指定されたら何もしないでfalseを返す)
		// 戻り値 true:削除した false:対象のindexが存在しなかった
		bool erase(size_t index) {
			if (m_type != Type::Array || index >= m_array.size()) return false;
			m_array.erase(m_array.begin()+ index);
			return true;
		}


		Type type()const noexcept {
			return m_type;
		}
		bool isType(Type t)const noexcept {
			return type() == t;
		}
		bool isNull()const noexcept {
			return isType(Type::Null);
		}

		// bool
		template<class U> U get(typename std::enable_if<std::is_same<U, bool>::value>::type* = nullptr) const noexcept {
			switch (m_type) {
			case Type::Bool:	return m_bool;
			//case Type::Int:	return static_cast<T>(m_bool);
			}
			return decltype(m_bool)();
		}
		// 実数
		template<class U> U get(typename std::enable_if<std::is_floating_point<U>::value>::type* = nullptr) const noexcept {
			switch (m_type) {
			case Type::Float:	return static_cast<U>(m_float);
			case Type::Int:		return static_cast<U>(m_int);
			}
			return decltype(m_float)();
		}
		// 整数
		template<class U> U get(typename std::enable_if<std::is_integral<U>::value && !std::is_same<U, bool>::value >::type* = nullptr) const noexcept {
			switch (m_type) {
			case Type::Bool:	return m_bool;
			case Type::Float:	return static_cast<U>(std::llroundl(m_float));
			case Type::Int:		return static_cast<U>(m_int);
			}
			return decltype(m_int)();
		}
		// 文字列
		template<class U> const U& get(typename std::enable_if<std::is_same<U, std::string>::value >::type* = nullptr) const noexcept {
			return m_type == Type::String ? m_string : m_emptyString;
		}
		// 配列
		template<class U> const U& get(typename std::enable_if<std::is_same<U, Array>::value >::type* = nullptr) const noexcept {
			return m_type == Type::Array ? m_array : m_emptyArray;
		}
		// 連想配列(オブジェクト)
		template<class U> const U& get(typename std::enable_if<std::is_same<U, Map>::value >::type* = nullptr) const noexcept {
			return m_type == Type::Map ? m_map : m_emptyMap;
		}

		// JSON Pointer
		struct Pointer {
			const std::string text;
			Pointer(const std::string& s) :text(s) {}
			Pointer(std::string&& s) :text(std::move(s)) {}
		};

		// 連想配列から要素を取得 (存在しないキーを指定されたら空実体を返す。例外発生しない)
		const JsonT& operator[](const Pointer& pointer)const {
			try{
				return at(pointer);
			}catch(std::out_of_range&){
			}
			return m_emptyJson;
		}
		//// 連想配列から要素(参照)を取得 (取得出来るようキーを追加する)
		// JsonT& operator[](const Pointer& pointer);	JsonPointer 版の実装はナシ(例外ナシを担保出来ない)

		// 連想配列から要素を取得 (存在しないキーを指定されたら throw std::out_of_range)
		const JsonT& at(const Pointer& pointer) const noexcept(false) {
			return const_cast<typename std::remove_const<typename std::remove_pointer<decltype(this)>::type>::type*>(this)->at(pointer);
		}
		JsonT& at(const Pointer& pointer) noexcept(false) {
			const auto tokens = [&] {
				using namespace std;
				if (pointer.text.empty()) return decltype(m_array)();
				const std::vector<string> tokens = [&] {
					static const regex re("/");
					const auto s = pointer.text + "/";
					return std::vector<string>{ sregex_token_iterator(s.cbegin(), s.cend(), re, -1), sregex_token_iterator() };
				}();

				auto json = parse(			// jsonパース処理を使う
					[&] {
						string json = "[";
						for (auto& s : tokens) {
							smatch m;
							static const regex r("^[0-9]+$");
							if (regex_search(s, m, r)) {	// 整数なら
								json += s + ",";
							} else {
								string result(s);
								struct {
									regex	re;
									string	dst;
								}static const tbl[] = {
									{regex("~0"),	R"(~)"	},
									{regex("~1"),	R"(/)"	},
									{regex("\\\r"),	R"(\r)"	},
									{regex("\\\t"),	R"(\t)"	},
									//{regex("\\/"),R"(\/)"	},
									{regex("\\\b"),	R"(\b)"	},
								};
								for (auto& i : tbl) {
									result = regex_replace(result, i.re, i.dst, regex_constants::match_default);
								}
								json += "\"" + result + "\",";
							}
						}
						json.pop_back();	// 末尾の "," を削除
						return json + "]";
					}());
				assert(json.type() == Type::Array);
				auto& v = json.ensureArray();
				if (v[0].type() == Type::String && v[0].template get<std::string>().empty()) {	// 先頭/の前の文字がある場合はNG
					v[0].clear();
				}
				return v;
			}();
			if (tokens.empty()) return *this;										// 空文字を指定されたら自身を返す
			if (!tokens[0].isNull()) throw std::out_of_range("invalid key");		// 先頭/の前の文字がある場合はNG
			auto* p = this;
			for (size_t i = 1; i < tokens.size(); i++) {
				auto& j = tokens[i];
				switch (j.type()) {
				case Type::Int:		p = &(p->at(j.template get<int>()));			break;
				case Type::String:	p = &(p->at(j.template get<std::string>()));	break;
				default:	throw std::out_of_range("invalid key");
				}
			}
			return *p;
		}

		// parse error
		struct ParseException : public std::runtime_error {
			const size_t position;
			ParseException(const std::string& msg, const std::string& json, const std::string::const_iterator& it)
				: std::runtime_error(msg + std::string(it, json.cend()).substr(0, 16))
				, position(it - json.cbegin())
			{}
		};

		// parse 設定
		struct ParseOptions {
			bool comment;		// true:コメント有効
			ParseOptions()
				:comment(true)
			{}
		};

		// JSON文字列をパース
		static JsonT parse(const std::string& sJson, const ParseOptions& opt = ParseOptions()) noexcept(false) {
			using namespace std;

			struct State {
				const std::string& json;
				JsonT				result;
				union {									// 次トークン情報
					struct {
						uint16_t	bFinish : 1;		// 文末
						uint16_t	bBegin : 1;			// 開始(オブジェクトあるいは配列の開始)
						uint16_t	end : 3;			// 終了 1:オブジェクト終了 2:オブジェクト終了あるいはカンマ 3:配列終了 4:配列終了あるいはカンマ
						uint16_t	objectKey : 2;		// オブジェクトのキー 1:キー(文字列) 2:キーの後のコロン
						uint16_t	value : 2;			// 値 1:オブジェクトの中の値 2:配列の中の値 3:それ以外の値
					};
					uint16_t		all = 0;
				}flags;
				vector<JsonT*>		parents;
				string::const_iterator	it;
				string::const_iterator	itLineEnd;

				State(const std::string& s)
					: json(s)
					, parents{ &result }
					, it(json.cbegin())
				{
					flags.bBegin = true;
					flags.value = 3;
				}

				// 値セット共通処理
				void setValue(const JsonT& value) {
					auto& parent = **parents.rbegin();
					switch (flags.value) {						// 値
					case 1:											// オブジェクトの中の値
						assert(parent.isNull());
						parent = value;
						parents.pop_back();
						flags.all = 0;								// 次トークン
						flags.end = 2;								// オブジェクト終了あるいはカンマ
						return;
					case 2:										// 配列の値
						if (!parent.isType(Type::Array)) throw ParseException("", json, it);
						parent[parent.m_array.size()] = value;
						flags.all = 0;								// 次トークン
						flags.end = 4;								// 配列終了あるいはカンマ
						return;
					case 3:										// それ以外の値
						parent = value;
						flags.all = 0;								// 次トークン
						flags.bFinish = true;						// 文末
						return;
					}
					throw ParseException("", json, it);
				};

				// 終了共通処理
				void setEnd() {
					if (parents.size() <= 1) {
						flags.all = 0;							// 次トークン
						flags.bFinish = true;					// 文末
					} else {
						parents.pop_back();
						switch ((*parents.rbegin())->type()) {
						case Type::Map:
							flags.all = 0;						// 次トークン
							flags.end = 2;						// オブジェクト終了あるいはカンマ
							break;
						case Type::Array:
							flags.all = 0;						// 次トークン
							flags.end = 4;						// 配列終了あるいはカンマ
							break;
						default:
							throw ParseException("", json, it);
						}
					}
				};
			}state(sJson);

			while (true) {
				try {
					if (state.flags.all == 0) throw ParseException("", state.json, state.it);
					if (state.parents.empty()) throw ParseException("", state.json, state.it);

					// 終了?
					if (state.it == state.json.cend()) {
						if (!state.flags.bFinish) throw ParseException("", state.json, state.it);
						break;
					}

					smatch m;

					// 先頭の行末を取得 (VisualC++ の regex は ^ が各行の先頭にヒットしてしまうので１行単位で処理する)
					state.itLineEnd = regex_search(state.it, state.json.cend(), m, regex("\n")) ? m[0].second : state.json.cend();

					// 空行なら次へ
					if (!regex_search(state.it, state.itLineEnd, m, regex(R"([^\s])"))) {
						state.it = state.itLineEnd;
						continue;
					}

					// トークン取得
					const string sToken = [&] {
						auto& f = state.flags;
						string s = R"((\/\/)|(\/\*))"		"|";			// コメント
						if (f.bFinish)	s += "\\S"			"|";			// 文末
						if (f.bBegin)	s += "\\{|\\["		"|";			// オブジェクトあるいは配列の開始
						switch (f.end) {
						case 0:									break;
						case 1:		s += "\\}"				"|"; break;		// オブジェクト終了
						case 2:		s += "\\}|\\,"			"|"; break;		// オブジェクト終了あるいはカンマ
						case 3:		s += "\\]"				"|"; break;		// 配列終了
						case 4:		s += "\\]|\\,"			"|"; break;		// 配列終了あるいはカンマ
						default:	assert(false);
						}
						switch (f.objectKey) {
						case 0:									break;
						case 1:		s += "\\\""				"|"; break;		// オブジェクトのキー(の最初の " )
						case 2:		s += ":"				"|"; break;		// オブジェクトのキーの後のコロン
						default:	assert(false);
						}
						if (f.value) s +=
							"\\\""										"|"	// 値 文字列(の最初の " )
							"true|false|null"							"|"	// 値 true|false|null
							"(-?[0-9]+(\\.[0-9]*)?([eE][+-]?[0-9]+)?)"	"|";// 値 浮動小数点表記 (頭に"+"が付く数値はエラー扱い)
						s.pop_back();	// 末尾の "|" を取る
						const regex re("^\\s*(" + s + ")");
						if (regex_search(state.it, state.itLineEnd, m, re)) {
							state.it = m[0].second;			// 次の位置
							return m[1].str();
						}
						return string();
					}();

					static const std::map<std::string, std::function<void(State&)>> mapToken{

						{"//", [](State& state) {		// コメント
							smatch m;
							if (!regex_search(state.it, state.json.cend(), m, regex(".*(\n|$)"))) {		// 改行まで
								throw ParseException("", state.json, state.it);
							}
							state.it = m[0].second;
						}},

						{"/*", [](State& state) {		// コメント
							smatch m;
							if (!regex_search(state.it, state.json.cend(), m, regex(R"(\*\/)"))) {		// 閉じるまで
								throw ParseException("", state.json, state.it);
							}
							state.it = m[0].second;
						}},

						{"{", [](State& state) {		// オブジェクト開始
							auto& parent = **state.parents.rbegin();
							if (parent.isType(Type::Array)) {				// 配列の中の要素なら
								auto& v = parent[parent.m_array.size()];	// 要素を追加
								v.ensureMap();								// オブジェクトに設定
								state.parents.push_back(&v);				// 親リストに自身を追加
							} else {
								if (!parent.isNull()) throw ParseException("", state.json, state.it);
								parent.ensureMap();							// オブジェクトに設定
							}
							state.flags.all = 0;
							state.flags.objectKey = 1;						// 次トークン(オブジェクトのキー)
							state.flags.end = 1;							// 次トークン(オブジェクト終了)
						}},

						{":", [](State& state) {		// オブジェクトのキーの後のコロン
							state.flags.all = 0;						// 次トークン
							state.flags.bBegin = true;					// オブジェクトあるいは配列の開始
							state.flags.value = 1;						// オブジェクトの中の値
						}},

						{"}", [](State& state) {		// オブジェクト終了
							auto& parent = **state.parents.rbegin();
							if (!parent.isType(Type::Map)) throw ParseException("", state.json, state.it);
							state.setEnd();
						}},

						{"[", [](State& state) {		// 配列開始
							auto& parent = **state.parents.rbegin();
							if (parent.isType(Type::Array)) {				// 配列の中の要素なら
								auto& v = parent[parent.m_array.size()];	// 要素を追加
								v.ensureArray();							// 配列に設定
								state.parents.push_back(&v);
							} else {
								if (!parent.isNull()) throw ParseException("", state.json, state.it);
								parent.ensureArray();					// 配列に設定
							}
							state.flags.all = 0;						// 次トークン
							state.flags.bBegin = true;					// オブジェクトあるいは配列の開始
							state.flags.value = 2;						// 配列の中の値
							state.flags.end = 3;						// 配列終了
						}},

						{ "]", [](State& state) {		// 配列終了
							auto& parent = **state.parents.rbegin();
							if (!parent.isType(Type::Array)) throw ParseException("", state.json, state.it);
							state.setEnd();
						} },

						{",", [](State& state) {		// カンマ
							switch (state.flags.end) {
							case 2:										// オブジェクト終了あるいはカンマ
								state.flags.all = 0;						// 次トークン
								state.flags.objectKey = 1;					// オブジェクトのキー
								return;
							case 4:										// 配列終了あるいはカンマ
								state.flags.all = 0;						// 次トークン
								state.flags.bBegin = true;					// オブジェクトあるいは配列の開始
								state.flags.value = 2;						// 配列の中の値
								return;
							}
							throw ParseException("", state.json, state.it);
						}},

						{"true", [](State& state) {		// true
							state.setValue(JsonT(true));
						}},

						{"false", [](State& state) {	// false
							state.setValue(JsonT(false));
						}},

						{"null", [](State& state) {		// null
							state.setValue(JsonT(nullptr));
						}},

						{"\"", [](State& state) {		// 文字列
							const string sText = [&]() {			// 文字列デコード
								string result;
								static const regex re("^(.*?)"	"("
									R"(\\u([dD][89abAB][0-9a-fA-F]{2})\\u([dD][c-fC-F][0-9a-fA-F]{2}))"	"|"		// UTF-16 文字コード サロゲートペア
									R"(\\u([0-9a-fA-F]{4}))"											"|"		// UTF-16 文字コード
									R"(\\")"					"|"
									R"(\\r)"					"|"
									R"(\\t)"					"|"
									R"(\\/)"					"|"
									R"(\\b)"					"|"
									R"(\\\\)"					"|"
									R"(")"						")");
								while (true) {
									smatch m;
									if (!regex_search(state.it, state.itLineEnd, m, re)) throw ParseException("", state.json, state.it);
									state.it = m[0].second;
									result += m[1].str();
									const string sToken = m[2].str();
									if (sToken == "\"") break;				// 文字列終了？

									static const std::map<string, string> mapReplace{		// エスケープ文字
										{ R"(\")",	"\"" },
										{ R"(\r)",	"\r" },
										{ R"(\t)",	"\t" },
										{ R"(\/)",	"/" },
										{ R"(\b)",	"\b" },
										{ R"(\\)",	"\\" },
									};
									const auto i = mapReplace.find(sToken);
									if (i != mapReplace.end()) {
										result += i->second;
										continue;
									}

									// "\uxxxx" UTF-16 文字コード
									const string u16a = m[3].str();			// 文字コード(サロゲートペア上位)
									const string u16b = m[4].str();			// 文字コード(サロゲートペア下位)
									const string u16 = m[5].str();			// 文字コード(非サロゲートペア)
									const vector<char16_t> c = u16.empty() ?
										vector<char16_t>{static_cast<char16_t>(stoi(u16a, nullptr, 16)), static_cast<char16_t>(stoi(u16b, nullptr, 16))} :
										vector<char16_t>{ static_cast<char16_t>(stoi(u16,nullptr,16)) };
									const u16string su16 = u16string(&c[0], c.size());
									const string su8 =						// UTF16 → UTF8
#ifdef _MSC_VER
										wstring_convert<codecvt_utf8_utf16<uint16_t>, uint16_t>().to_bytes(reinterpret_cast<const uint16_t*>(su16.c_str()));
#else
										wstring_convert<codecvt_utf8_utf16<char16_t>, char16_t>().to_bytes(su16.c_str());
#endif
									result += su8;
								}
								return result;
							}();

							if (state.flags.objectKey == 1) {					// 処理したのはオブジェクトのキーなら
								auto& parent = **state.parents.rbegin();
								assert(parent.isType(Type::Map));
								auto& v = parent[sText];
								v.clear();									// 2度目以降の登場は後勝ち(JSONの仕様)
								state.parents.push_back(&v);
								state.flags.all = 0;						// 次トークン
								state.flags.objectKey = 2;					// オブジェクトのキーの後のコロン
								return;
							}

							state.setValue(JsonT(sText));
						}},

					};
					const auto i = mapToken.find(sToken);
					if (i != mapToken.end()) {
						i->second(state);
						continue;
					}

					// それ以外は数値
					state.setValue(
						[&] {
							smatch m;
							static const regex r("^-?[0-9]+$");
							if (regex_search(sToken, m, r)) {	// 整数なら
								try {
									return JsonT(static_cast<intmax_t>(stoll(sToken)));
								} catch (const exception&) {
								}
							}
							return JsonT(stod(sToken));			// doubleで処理
						}());

				} catch (const ParseException&) {
					throw;
				} catch (const exception& e) {
					throw ParseException(e.what(), state.json, state.it);
				} catch (...) {
					throw;
				}
			}//while(true)

			return state.result;
		}

		// JSON文字列を出力
		std::string stringify()const {
			using namespace std;
			struct F {
				static string f(const JsonT& j) {
					auto fEscape = [](const string& s) {	// 文字列エスケープ処理
						string result(s);
						struct {
							regex	re;
							string	dst;
						}static const tbl[] = {
							{regex("\\\\"),	R"(\\)"	},
							{regex("\\\""),	R"(\")"	},
							{regex("\\\r"),	R"(\r)"	},
							{regex("\\\t"),	R"(\t)"	},
							{regex("\\/"),	R"(\/)"	},
							{regex("\\\b"),	R"(\b)"	},
						};
						for (auto& i : tbl) {
							result = regex_replace(result, i.re, i.dst, regex_constants::match_default);
						}
						return result;
					};

					string s;
					switch (j.type()) {
					case Type::Null:
						return "null";
					case Type::Bool:
						return j.m_bool ? "true" : "false";
					case Type::Float:
						return to_string(j.m_float);
					case Type::Int:
						return to_string(j.m_int);
					case Type::String:
						return "\"" + fEscape(j.m_string) + "\"";
					case Type::Array:
						s = "[";
						for (auto& i : j.m_array) {
							s += F::f(i) + ",";
						}
						if (j.m_array.size() >= 1) s.pop_back();	// 末尾の "," を削除
						return s + "]";
					case Type::Map:
						s = "{";
						for (auto& i : j.m_map) {
							s += "\"" + fEscape(i.first) + "\":" + F::f(i.second) + ",";
						}
						if (j.m_map.size() >= 1) s.pop_back();	// 末尾の "," を削除
						return s + "}";
					}
					assert(false);
					return string();
				}
			};
			return F::f(*this);
		}
	};
	template <class T> const JsonT<T>					JsonT<T>::m_emptyJson;
	template <class T> const std::string				JsonT<T>::m_emptyString;
	template <class T> const typename JsonT<T>::Array	JsonT<T>::m_emptyArray;
	template <class T> const typename JsonT<T>::Map		JsonT<T>::m_emptyMap;
	typedef JsonT<> Json;
}

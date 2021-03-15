# JSON パーサー

[英語](/README.md)

## Description

JSON パーサーです。C++での実装です。

+ JSON 仕様に沿ったデータ構造クラスに、パース(parse)と出力(stringify)機能を付加したものです。
+ ヘッダーファイルのみで動作します。
+ C++11 でビルド可です。boost などの外部ライブラリには依存していません。
+ 参照や編集等で例外は発生しない設計です。( at() 関数を除く)
  + 範囲外の読み込みはデフォルト値が取得され、書き込みの場合は要素を作成します。
+ 入出力は std::string（UTF-8）のみ対応です。
+ javascript と違い、数値は実数(double)と整数(std::intmax_t)に区別して処理しています。
+ JSON5 の一部仕様を実装しています。
  + コメント付きJSONをパース可能です。(オプションで無効に出来ます)
+ JSON Pointer を実装しています。
+ ストリーム入力のパース処理には非対応です。


## Requirement

c++11 環境で動作します。動作に必要なものは特にありません。

以下の環境で動作確認しています。
+ linux g++
+ windows VisualStudio 2015,2017,2019

## Usage

Json.h をインクルードすることで使用可です。

## example

```c++

#include "Json.h"

try {
    const rlib::Json j = rlib::Json::parse(             // JSON 文字列から構築
        u8R"({
            "n" : -123.456e+2,
            "list":[
                32,
                "ABC"
            ],
            "b": true,
            "c": null
        })");
    double d0 = j["n"].get<double>();                   // -123.456e+2 を取得
    double da = j.at("n").get<double>();                // at() で参照する記述です。（範囲外の場合に例外が発生します）
    double d1 = j["e"].get<double>();                   // 0.0 を取得 (存在しない位置を指定したのでデフォルト値が取れる)
    std::intmax_t n1 = j["n"].get<std::intmax_t>();     // -12346 を取得 (double値を四捨五入した整数値が取れます)
    std::string s0 = j["list"][1].get<std::string>();   // "ABC" を取得
    std::string sa = j.at(rlib::Json::Pointer("/list/1")).get<std::string>();	// JSON Pointerで指定する記述です。
    std::string s1 = j["ary"][9].get<std::string>();    // 空文字を取得 (存在しない位置を指定したのでデフォルト値が取れる)
    rlib::Json list = j["list"];                        // "list"以下をコピー(複製)
    list[10]["add"] = 123;                              // [10]の位置に {"add":123} を 追加 ( 配列[2～9]の位置は null で埋められる)
    bool compare = list == j["list"];                   // 比較です。false が返ります。
    std::string json = list.stringify();                // JSON 文字列を取得
    rlib::Json &c = list.at(11);                        // at() で参照すると範囲外の場合に例外が発生します
} catch (rlib::Json::ParseException& e) {       // パース失敗
    std::cerr << e.what() << std::endl;
} catch (std::out_of_range& e) {                // 範囲外参照
    std::cerr << e.what() << std::endl;
}
```

## Feature and Limitations

- BOOST.TEST を使ったテストコードも含んでいます。Json_test.cpp
- コードのインデントはスペースではなくタブです。github では URL 末尾に "?ts=4" を付けてタブ4で参照ください。すみません。

## Licence

[LICENSE](/LICENSE)

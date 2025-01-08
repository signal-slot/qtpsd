// Copyright (C) 2024 Signal Slot Inc.
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qpsdenginedataparser.h"

#include <QtCore/qiodevice.h>
#include <QtCore/qcbormap.h>
#include <QtCore/qcborarray.h>

QT_BEGIN_NAMESPACE

/**
 * @brief QPsdEngineDataParser::Private クラスは、EngineData のパース処理を担当します。
 */
class QPsdEngineDataParser::Private
{
public:
    /**
     * @brief コンストラクタ
     * @param data パース対象のデータ
     */
    Private(const QByteArray &data)
        : m_data(data), m_pos(0), m_length(data.length()) {}

    /**
     * @brief パース処理の開始
     * @param map パース結果を格納する QCborMap のポインタ
     * @return パースが成功した場合は空の ParseError、失敗した場合はエラーメッセージを持つ ParseError を返します
     */
    ParseError parse(QCborMap* map) {
        skipWhitespace();

        // '<<' の確認
        if (!matchString("<<"))
            return error("EngineData must start with '<<'"_L1);

        // 辞書のパース
        ParseError dictError = parseDictionary(map);
        if (dictError)
            return dictError;

        skipWhitespace();

        // データの終端確認
        if (m_pos != m_length) {
            qDebug() << m_pos << m_length;
            return error(u"辞書の終了後に予期せぬデータが存在します。"_s);
        }

        // パース成功
        return success();
    }

private:
    const QByteArray &m_data;    ///< 入力データ
    int m_pos;                ///< 現在のパース位置
    int m_length;             ///< データの長さ

    /**
     * @brief 次の文字を取得します。
     * @return 取得した char。エラーの場合は 0 を返します。
     */
    char getNextChar() {
        if (m_pos >= m_length)
            return 0;

        return m_data.at(m_pos++);
    }

    /**
     * @brief 次の文字を先読みします。
     * @return 先読みした char。エラーの場合は 0 を返します。
     */
    char peekNextChar() const {
        if (m_pos >= m_length)
            return 0;

        return m_data.at(m_pos);
    }

    /**
     * @brief ホワイトスペースをスキップします。
     */
    void skipWhitespace() {
        while (m_pos < m_length && QChar::fromLatin1(m_data.at(m_pos)).isSpace())
            ++m_pos;
    }

    /**
     * @brief 指定された文字列と現在のバッファ位置を照合します。
     * @param str 照合する文字列。
     * @return 一致した場合は true、そうでない場合は false。
     */
    bool matchString(const QByteArray &str) {
        if (m_pos + str.length() > m_length)
            return false;

        const auto substring = m_data.mid(m_pos, str.length());
        if (substring == str) {
            m_pos += str.length();
            return true;
        }
        return false;
    }

    /**
     * @brief 辞書をパースします（'<<' と '>>' で囲まれた部分）。
     * @param map パース結果を格納する QCborMap のポインタ。
     * @return パースに失敗した場合はエラーメッセージを持つ ParseError を返します。成功した場合は空の ParseError を返します。
     */
    ParseError parseDictionary(QCborMap* map) {
        while (m_pos < m_length) {
            skipWhitespace();

            char ch = peekNextChar();
            if (QChar::fromLatin1(ch).isNull())
                return error("Unexpected end of data while parsing dictionary"_L1);

            if (ch == '>'_L1) {
                // '>>' の確認
                if (!matchString(">>"))
                    return error("Invalid dictionary end '>>'"_L1);
                return success(); // 辞書の終了
            }

            // プロパティ名のパース
            QString key;
            ParseError keyError = parsePropertyName(&key);
            if (keyError)
                return keyError;

            // 値のパース
            QCborValue value;
            ParseError valueError = parseValue(&value);
            if (valueError)
                return valueError;

            // マップに挿入
            map->insert(key, value);
        }

        // 到達しないはず
        return error(u"不明なエラーが発生しました。"_s);
    }

    /**
     * @brief プロパティ名をパースします（'/' で始まる）。
     * @param key パースしたプロパティ名を格納する QString のポインタ。
     * @return パースに失敗した場合はエラーメッセージを持つ ParseError を返します。成功した場合は空の ParseError を返します。
     */
    ParseError parsePropertyName(QString* key) {
        char ch = getNextChar();
        if (ch != '/')
            return error("Property name must start with '/'"_L1);

        QByteArray keyStr;
        while (m_pos < m_length) {
            ch = peekNextChar();
            if (QChar::fromLatin1(ch).isNull() || !QChar::fromLatin1(ch).isLetterOrNumber())
                break;
            keyStr += getNextChar();
        }

        if (keyStr.isEmpty())
            return error("Property name is empty"_L1);

        *key = QString::fromUtf8(keyStr);
        return success(); // 成功
    }

    /**
     * @brief 値をパースします。
     * @param value パースした値を格納する QCborValue のポインタ。
     * @return パースに失敗した場合はエラーメッセージを持つ ParseError を返します。成功した場合は空の ParseError を返します。
     */
    ParseError parseValue(QCborValue* value) {
        skipWhitespace();

        char ch = peekNextChar();
        if (QChar::fromLatin1(ch).isNull())
            return error("Unexpected end of data while parsing value"_L1);

        if (ch == '<'_L1) { // 辞書
            // '<<' の確認
            if (!matchString("<<"))
                return error("Invalid dictionary start '<<'"_L1);

            QCborMap subMap;
            ParseError dictError = parseDictionary(&subMap);
            if (dictError)
                return dictError;

            *value = QCborValue(subMap);
            return success(); // 成功
        } else if (ch == '['_L1) { // 配列
            // '[' の消費
            getNextChar(); // '['

            QCborArray array;
            while (m_pos < m_length) {
                skipWhitespace();

                ch = peekNextChar();
                if (QChar::fromLatin1(ch).isNull())
                    return error(u"配列のパース中に予期せぬデータの終端に到達しました。"_s);

                if (ch == ']'_L1) { // 配列の終了
                    getNextChar(); // ']'
                    break;
                }

                // 配列要素のパース
                QCborValue element;
                ParseError elementError = parseValue(&element);
                if (elementError)
                    return elementError;

                array.append(element);
            }

            *value = QCborValue(array);
            return success(); // 成功
        } else if (ch == '('_L1) { // 文字列
            QString str;
            ParseError strError = parseString(&str);
            if (strError)
                return strError;

            *value = QCborValue(str);
            return success(); // 成功
        } else if (QChar::fromLatin1(ch).toLower() == 't'_L1 || QChar::fromLatin1(ch).toLower() == 'f'_L1) { // ブール値
            QByteArray boolStr;
            ParseError boolError = parseBoolean(&boolStr);
            if (boolError)
                return boolError;

            if (boolStr == "true") {
                *value = QCborValue(true);
                return success(); // 成功
            } else if (boolStr == "false") {
                *value = QCborValue(false);
                return success(); // 成功
            }
            return error(u"無効なブール値です。"_s);
        } else { // 数値
            QByteArray numStr;
            ParseError numError = parseNumber(&numStr);
            if (numError)
                return numError;

            bool ok = false;
            double num = numStr.toDouble(&ok);
            if (!ok)
                return error("Invalid number format: '%1'"_L1.arg(numStr));

            if (numStr.contains('.')) {
                *value = QCborValue(num);
            } else {
                qint64 intVal = numStr.toLongLong(&ok);
                if (ok)
                    *value = QCborValue(intVal);
                else
                    *value = QCborValue(num);
            }

            return success(); // 成功
        }
        return error("Invalid value type"_L1); // Add missing return statement
    }

    /**
     * @brief 文字列をパースします（'(' と ')' で囲まれ、特定のプレフィックスを持つ）。
     * @param str パースした文字列を格納する QString のポインタ。
     * @return パースに失敗した場合はエラーメッセージを持つ ParseError を返します。成功した場合は空の ParseError を返します。
     */
    ParseError parseString(QString* str) {
        char ch = getNextChar();
        if (ch != '(')
            return error("String must start with '('"_L1);

        // 特殊なプレフィックス (˛ˇ) の処理
        const auto prefix4 = m_data.mid(m_pos, 4);
        if (prefix4 == "\xCB\x9B\xCB\x87") { // ˛ˇ == ogonek caron
            m_pos += 4; // '˛ˇ' の消費
        }

        bool isUtf16 = false;
        // BOM Check
        if (m_data.mid(m_pos, 2) == "\xFE\xFF") {
            m_pos += 2; // BOM の消費
            isUtf16 = true;
        }

        QByteArray strContent;
        bool escaped = false;
        while (m_pos < m_length) {
            ch = getNextChar();
            if (escaped) {
                escaped = false;
            } else if (ch == '\\') {
                escaped = true;
                continue;
            } else {
                if (ch == ')') {
                    break;
                }
            }
            strContent += ch;
        }

        QStringDecoder decoder(QStringDecoder::Utf16BE);
        if (isUtf16)
            *str = decoder.decode(strContent);
        else
            *str = QString::fromLatin1(strContent);

        return success(); // 成功
    }

    /**
     * @brief ブール値をパースします（'true' または 'false'）。
     * @param boolStr パースしたブール値文字列を格納する QByteArray のポインタ。
     * @return パースに失敗した場合はエラーメッセージを持つ ParseError を返します。成功した場合は空の ParseError を返します。
     */
    ParseError parseBoolean(QByteArray* boolStr) {
        QString boolQString;
        while (m_pos < m_length && QChar::fromLatin1(m_data.at(m_pos)).isLetter()) {
            boolQString += QLatin1Char(m_data.at(m_pos));
            ++m_pos;
        }

        QString boolLower = boolQString.toLower();
        if (boolLower == "true"_L1) {
            *boolStr = "true"_ba;
            return success(); // 成功
        } else if (boolLower == "false"_L1) {
            *boolStr = "false"_ba;
            return success(); // 成功
        }
        return error(u"無効なブール値です。"_s);
    }

    /**
     * @brief 数値をパースします。
     * @param numStr パースした数値文字列を格納する QByteArray のポインタ。
     * @return パースに失敗した場合はエラーメッセージを持つ ParseError を返します。成功した場合は空の ParseError を返します。
     */
    ParseError parseNumber(QByteArray* numStr) {
        QByteArray numberStr;
        if (peekNextChar() == '-')
            numberStr += getNextChar(); // '-' の消費

        while (m_pos < m_length) {
            char ch = peekNextChar();
            if (QChar::fromLatin1(ch).isDigit() || ch == '.'_L1)
                numberStr += getNextChar();
            else
                break;
        }

        if (numberStr.isEmpty() || numberStr == "-")
            return error("Number expected but not found"_L1);

        *numStr = numberStr;
        return success(); // 成功
    }

    /**
     * @brief エラーメッセージを設定します。
     * @param message エラーメッセージ。
     * @return 設定したエラーメッセージを持つ ParseError を返します。
     */
    ParseError error(const QString &message) {
        qWarning() << "QPsdEngineDataParser::ParseError -" << message;
        return ParseError(message);
    }

    /**
     * @brief パース成功時の ParseError を返します。
     * @return 空の ParseError を返します。
     */
    ParseError success() {
        return ParseError();
    }
};

/**
 * @brief QPsdEngineDataParser クラスの静的メソッド。QString から EngineData をパースして QCborMap に変換します。
 * @param data パース対象の QByteArray
 * @param error エラー情報を格納する ParseError のポインタ（オプション）。
 * @return パース結果を格納した QCborMap。パースに失敗した場合は空の QCborMap を返します。
 */
QCborMap QPsdEngineDataParser::parseEngineData(const QByteArray &data, ParseError* error)
{
    QCborMap map;
    Private parser(data);
    ParseError parseError = parser.parse(&map);
    if (!parseError)
        return map; // パース成功

    if (error)
        *error = parseError;
    return QCborMap(); // パース失敗
}

QT_END_NAMESPACE

#include "lexer.h"

using namespace std;

#include <iostream>

namespace parse {

	bool operator==(const Token& lhs, const Token& rhs) {
		using namespace token_type;

		if (lhs.index() != rhs.index()) {
			return false;
		}
		if (lhs.Is<Char>()) {
			return lhs.As<Char>().value == rhs.As<Char>().value;
		}
		if (lhs.Is<Number>()) {
			return lhs.As<Number>().value == rhs.As<Number>().value;
		}
		if (lhs.Is<String>()) {
			return lhs.As<String>().value == rhs.As<String>().value;
		}
		if (lhs.Is<Id>()) {
			return lhs.As<Id>().value == rhs.As<Id>().value;
		}
		return true;
	}

	bool operator!=(const Token& lhs, const Token& rhs) {
		return !(lhs == rhs);
	}

	std::ostream& operator<<(std::ostream& os, const Token& rhs) {
		using namespace token_type;

#define VALUED_OUTPUT(type) \
		if (auto p = rhs.TryAs<type>()) return os << #type << '{' << p->value << '}';

		VALUED_OUTPUT(Number);
		VALUED_OUTPUT(Id);
		VALUED_OUTPUT(String);
		VALUED_OUTPUT(Char);

#undef VALUED_OUTPUT

#define UNVALUED_OUTPUT(type) \
		if (rhs.Is<type>()) return os << #type;

		UNVALUED_OUTPUT(Class);
		UNVALUED_OUTPUT(Return);
		UNVALUED_OUTPUT(If);
		UNVALUED_OUTPUT(Else);
		UNVALUED_OUTPUT(Def);
		UNVALUED_OUTPUT(Newline);
		UNVALUED_OUTPUT(Print);
		UNVALUED_OUTPUT(Indent);
		UNVALUED_OUTPUT(Dedent);
		UNVALUED_OUTPUT(And);
		UNVALUED_OUTPUT(Or);
		UNVALUED_OUTPUT(Not);
		UNVALUED_OUTPUT(Eq);
		UNVALUED_OUTPUT(NotEq);
		UNVALUED_OUTPUT(LessOrEq);
		UNVALUED_OUTPUT(GreaterOrEq);
		UNVALUED_OUTPUT(None);
		UNVALUED_OUTPUT(True);
		UNVALUED_OUTPUT(False);
		UNVALUED_OUTPUT(Eof);

#undef UNVALUED_OUTPUT

		return os << "Unknown token :("sv;
	}

	Lexer::Lexer(std::istream& input) {
		LoadTokens(input);
	}

	const Token& Lexer::CurrentToken() const {
		return tokens_[current_token_];
	}

	Token Lexer::NextToken() {
		return tokens_[current_token_ + 1 >= tokens_.size() ? current_token_ : ++current_token_];
	}

	int LoadNumber(std::istream& input) {
		std::string parsed_num;

		// Считывает в parsed_num очередной символ из input
		auto read_char = [&parsed_num, &input] {
			parsed_num += static_cast<char>(input.get());
			if (!input) {
				//throw ParsingError("Failed to read number from stream"s);
			}
		};

		// Считывает одну или более цифр в parsed_num из input
		auto read_digits = [&input, read_char] {
			if (!std::isdigit(input.peek())) {
				//throw ParsingError("A digit is expected"s);
			}
			while (std::isdigit(input.peek())) {
				read_char();
			}
		};

		read_digits();

		return std::stoi(parsed_num);
	}

	static const std::unordered_map<std::string, Token> KEY_WORDS_MAP = {
		{ "class"s,		token_type::Class() },
		{ "return"s,	token_type::Return() },
		{ "if"s,		token_type::If() },
		{ "else"s,		token_type::Else() },
		{ "def"s,		token_type::Def() },
		{ "print"s,		token_type::Print() },
		{ "or"s,		token_type::Or() },
		{ "None"s,		token_type::None() },
		{ "and"s,		token_type::And() },
		{ "not"s,		token_type::Not() },
		{ "True"s,		token_type::True() },
		{ "False"s,		token_type::False() }
	};

	Token Lexer::LoadIdentifier(std::istream& input) {
		// Считываем значение
		std::string s;
		char c;
		while (true) {
			input.get(c);
			if (c == '#') {
				input.ignore(numeric_limits<streamsize>::max(), '\n');
				if (!input.eof()) {
					input.putback('\n');
				}
				break;
			}
			if (c == ':' && tokens_.back().Is<token_type::Class>()) {
				input.putback(c);
				break;
			}
			if (c == '(' || c == ')' || c == ',' || c == '.' || c == ':') {
				input.putback(c);
				break;
			}
			if (c == '\n') {
				input.putback(c);
				break;
			}
			if (c == ' ' || input.eof()) {
				break;
			}
			s.push_back(c);
		}
		// Определяем является ли он ключевым словом
		if (KEY_WORDS_MAP.count(s)) {
			return KEY_WORDS_MAP.at(s);
		}

		return token_type::Id{ s };
	}

	std::string LoadString(std::istream& input) {
		std::string str;

		char starting_symbol = input.get();
		char current_symbol = input.get();

		while (current_symbol != starting_symbol) {
			if (current_symbol == '\\') {
				input.get(current_symbol);
				switch (current_symbol) {
				case 'n':
					str.push_back('\n');
					break;
				case 't':
					str.push_back('\t');
					break;				
				case '\"':
					str.push_back('\"');
					break;
				case '\'':
					str.push_back('\'');
					break;
				case '\\':
					str.push_back('\\');
					break;
				default:
					break;
				}
			}
			else {
				str.push_back(current_symbol);
			}
			input.get(current_symbol);
		}

		return str;
	}

	inline bool IsNextSpace(std::istream& input) {
		return input.peek() == ' ';
	}

	void Lexer::LoadTokens(std::istream& input) {
		char c;
		while (true) {
			input.get(c);

			// Считываем наличие/отсутствие отступов
			if (tokens_.empty() || tokens_.back().Is<token_type::Newline>()) {
				size_t space_num = 0;

				while (c == ' ') {
					++space_num;
					input.get(c);
				}
				size_t new_indent = space_num / 2;

				if (new_indent > indent_) {
					for (size_t i = 0; i < (new_indent - indent_); ++i) {
						tokens_.emplace_back(token_type::Indent{});
					}
				}
				else if (new_indent < indent_) {
					for (size_t i = 0; i < (indent_ - new_indent); ++i) {
						tokens_.emplace_back(token_type::Dedent{});
					}
				}
				old_indent_ = indent_;
				indent_ = new_indent;
			}
			// Конец файла
			if (input.eof()) {
				if (!tokens_.empty() && (!tokens_.back().Is<token_type::Newline>()
					&& !tokens_.back().Is<token_type::Indent>()
					&& !tokens_.back().Is<token_type::Dedent>())) {
					tokens_.emplace_back(token_type::Newline{});
				}
				tokens_.emplace_back(token_type::Eof{});
				break;
			}
			// Игнорируем все символы после символа # до конца строки
			if (c == '#') {
				input.ignore(numeric_limits<streamsize>::max(), '\n');
				if (!input.eof()) {
					input.putback('\n');
				}
				continue;
			}

			// Новая строка
			if (c == '\n') {
				// Если символ конца строки, но кроме отступов небыло никаких данных
				// необходимо стереть все Indent/Dedent и вернуть значение отступа к предыдущему
				if (!tokens_.empty() && (tokens_.back().Is<token_type::Indent>() || tokens_.back().Is<token_type::Dedent>())) {
					while (!tokens_.empty() && !tokens_.back().Is<token_type::Newline>()) {
						tokens_.pop_back();
					}
					indent_ = old_indent_;
				}
				else if (!(tokens_.empty() || tokens_.back().Is<token_type::Newline>())) {
					tokens_.emplace_back(token_type::Newline{});
				}
			}
			// Арифмитические символы и символы сравнения
			else if (c == '!' && input.peek() == '=') {
				tokens_.emplace_back(token_type::NotEq{});
				input.ignore(1);
			}
			else if (c == '=' && input.peek() == '=') {
				tokens_.emplace_back(token_type::Eq{});
				input.ignore(1);
			}
			else if (c == '<' && input.peek() == '=') {
				tokens_.emplace_back(token_type::LessOrEq{});
				input.ignore(1);
			}
			else if (c == '>' && input.peek() == '=') {
				tokens_.emplace_back(token_type::GreaterOrEq{});
				input.ignore(1);
			}

			else if (c == '+' || c == '-' || c == '*' || c == '/') {
				tokens_.emplace_back(token_type::Char{ c });
			}
			else if (c == '=' || c == '<' || c == '>') {
				tokens_.emplace_back(token_type::Char{ c });
			}
			// Начинается с цифры или знака минус
			else if (std::isdigit(c)) {
				input.putback(c);
				tokens_.emplace_back(token_type::Number{ LoadNumber(input) });
			}
			// Идентификатор начинается с буквы или символа подчеркивания
			else if (std::isalpha(c) || c == '_') {
				input.putback(c);
				tokens_.emplace_back(LoadIdentifier(input));
			}
			// Строки начинаются с одинарных или двойных кавычек
			else if (c == '\'' || c == '"') {
				input.putback(c);
				tokens_.emplace_back(token_type::String{ LoadString(input) });
			}

			else if (c == '=' && IsNextSpace(input)) {
				tokens_.emplace_back(token_type::Char{ c });
			}
			else if (c != ' ') {
				tokens_.emplace_back(token_type::Char{ c });
			}
		}
	}

}  // namespace parse
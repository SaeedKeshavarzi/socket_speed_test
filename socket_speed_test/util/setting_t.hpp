#ifndef _SETTING_T_HPP_
#define _SETTING_T_HPP_

#include <exception>
#include <assert.h>
#include <iostream>
#include <sstream>
#include <fstream>

class setting_t // virtual class
{
public:
	enum class type_t : int
	{
		vt_none = 0,
		// scalar types -> bool, int, int64, double, float or string
		vt_bool,
		vt_int,
		vt_int64,
		vt_double,
		vt_float,
		vt_string,
		// aggregate types -> vector, list or group
		vt_vector, // sequence of zero, one or most setting_t item with same type
		vt_group // sequence of zero, one or most setting_t item with different type
	};

	virtual const type_t value_type() const = 0;
	inline const std::string & label() const { return label_; }
	inline std::string & label() { return label_; }

	// copy cast
	virtual operator const bool() const {
		assert(value_type() == type_t::vt_bool);
		throw new std::invalid_argument("Not Implemented!");
	}

	virtual operator const int() const {
		assert(value_type() == type_t::vt_int);
		throw new std::invalid_argument("Not Implemented!");
	}

	virtual operator const long long() const {
		assert((value_type() == type_t::vt_int) || (value_type() == type_t::vt_int64));
		throw new std::invalid_argument("Not Implemented!");
	}

	virtual operator const double() const {
		assert(value_type() == type_t::vt_double);
		throw new std::invalid_argument("Not Implemented!");
	}

	virtual operator const float() const {
		assert(value_type() == type_t::vt_float);
		throw new std::invalid_argument("Not Implemented!");
	}

	virtual operator const std::string() const {
		assert(value_type() == type_t::vt_string);
		throw new std::invalid_argument("Not Implemented!");
	}

	// reference cast
	virtual operator bool&() {
		assert(value_type() == type_t::vt_bool);
		throw new std::invalid_argument("Not Implemented!");
	}

	virtual operator int&() {
		assert(value_type() == type_t::vt_int);
		throw new std::invalid_argument("Not Implemented!");
	}

	virtual operator long long&() {
		assert((value_type() == type_t::vt_int) || (value_type() == type_t::vt_int64));
		throw new std::invalid_argument("Not Implemented!");
	}

	virtual operator double&() {
		assert(value_type() == type_t::vt_double);
		throw new std::invalid_argument("Not Implemented!");
	}

	virtual operator float&() {
		assert(value_type() == type_t::vt_float);
		throw new std::invalid_argument("Not Implemented!");
	}

	virtual operator std::string&() {
		assert(value_type() == type_t::vt_string);
		throw new std::invalid_argument("Not Implemented!");
	}

	// assign operator
	virtual setting_t & operator=(bool value) {
		assert(value_type() == type_t::vt_bool);
		throw new std::invalid_argument("Not Implemented!");
	}

	virtual setting_t & operator=(int value) {
		assert(value_type() == type_t::vt_int);
		throw new std::invalid_argument("Not Implemented!");
	}

	virtual setting_t & operator=(long long value) {
		assert(value_type() == type_t::vt_int64);
		throw new std::invalid_argument("Not Implemented!");
	}

	virtual setting_t & operator=(double value) {
		assert(value_type() == type_t::vt_double);
		throw new std::invalid_argument("Not Implemented!");
	}

	virtual setting_t & operator=(float value) {
		assert(value_type() == type_t::vt_float);
		throw new std::invalid_argument("Not Implemented!");
	}

	virtual setting_t & operator=(std::string value) {
		assert(value_type() == type_t::vt_string);
		throw new std::invalid_argument("Not Implemented!");
	}

	virtual void scan(bool suggest_current_value = false, int tab_level = 0) = 0;
	virtual void print(std::ostream & ous = std::cout, int tab_level = 0) const = 0;
	virtual void fread(std::istream & ins) = 0;

	bool read_file(const std::string & path)
	{
		std::ifstream ins;

		ins.open(path, std::ios::in);
		if (!ins.is_open())
		{
			return false;
		}

		fread(ins);

		ins.close();
		return true;
	}

	bool write_file(const std::string & path)
	{
		std::ofstream ous;

		ous.open(path, std::ios::out);
		if (!ous.is_open())
		{
			return false;
		}

		print(ous);

		ous.close();
		return true;
	}

protected:
	setting_t(const std::string & _label = "") : label_{ _label }
	{
		std::replace(label_.begin(), label_.end(), ':', '=');
	}

	template<typename _Ty, bool is_string = std::is_same<_Ty, std::string>::value> struct read_value_t;

	template<typename _Ty> struct read_value_t<_Ty, false> {
		static void read(_Ty & value, std::istream & ins = std::cin)
		{
			ins >> value;
		}
	};

	template<typename _Ty> struct read_value_t<_Ty, true> {
		static void read(_Ty & value, std::istream & ins = std::cin, const char delim = '\n')
		{
			std::getline(ins, value, delim);
			value = trim(value);
		}
	};

private:
	std::string label_;

	static std::string trim(const std::string & str)
	{
		const std::string white_chars = " \t";

		const auto str_begin = str.find_first_not_of(white_chars);
		if (str_begin == std::string::npos)
		{
			return "";
		}

		const auto str_end = str.find_last_not_of(white_chars);
		return str.substr(str_begin, str_end - str_begin + 1);
	}
};

template<typename _Tv>
class scalar_t : public setting_t // bool, int, int64, double, float or string
{
	static_assert(
		std::is_integral<_Tv>::value ||
		std::is_floating_point<_Tv>::value ||
		std::is_same<_Tv, std::string>::value,
		"Invalid Type in Setting Scalar!");

public:
	using my_value_type = _Tv;
	using my_type = scalar_t<_Tv>;

	scalar_t(const std::string & _label = "") : setting_t(_label) {  }
	scalar_t(const std::string & _label, my_value_type _value) : setting_t(_label), value_{ _value } {  }

	const type_t value_type() const
	{
		if (std::is_same<my_value_type, bool>::value)
			return type_t::vt_bool;

		if (std::is_same<my_value_type, int>::value)
			return type_t::vt_int;

		if (std::is_same<my_value_type, long long>::value)
			return type_t::vt_int64;

		if (std::is_same<my_value_type, double>::value)
			return type_t::vt_double;

		if (std::is_same<my_value_type, float>::value)
			return type_t::vt_float;

		if (std::is_same<my_value_type, std::string>::value)
			return type_t::vt_string;

		throw new std::invalid_argument("Invalid Type in Setting Scalar!");
	}

	operator const my_value_type() const
	{
		return value_;
	}

	operator my_value_type&()
	{
		return value_;
	}

	my_type & operator=(my_value_type value)
	{
		value_ = value;
		return *this;
	}

	const my_value_type & operator()() const
	{
		return value_;
	}

	my_value_type & operator()()
	{
		return value_;
	}

	void scan(bool suggest_current_value = false, int tab_level = 0)
	{
		auto pfx = std::string(tab_level * 2, ' ');

		do
		{
			if (suggest_current_value)
			{
				char ind; // indicator
				do {
					std::cout << pfx << label() << ": " << value_ << "; OK? (Yes: Press Enter|Edit: 'e' + Enter) " << std::flush;

					do { char ch; while (std::cin.readsome(&ch, 1) != 0); } while (0); // FLUSH_OUT();
					ind = tolower(getchar());
					do { char ch; while (std::cin.readsome(&ch, 1) != 0); } while (0); // FLUSH_OUT();
				} while ((ind != 10 /* Enter -> OK */) && (ind != 'e' /* -> Edit */));

				if (ind == 10 /* Enter -> OK */)
				{
					return;
				}
			}

			std::cout << pfx << label() << "? " << std::flush;
			do { char ch; while (std::cin.readsome(&ch, 1) != 0); } while (0); // FLUSH_OUT();
			read_value_t<my_value_type>::read(value_, std::cin);
			suggest_current_value = true;
		} while (true);
	}

	void print(std::ostream & ous = std::cout, int tab_level = 0) const
	{
		ous << std::string(tab_level * 2, ' ') << label() << ": " << value_ << std::endl << std::flush;
	}

	void fread(std::istream & ins)
	{
		char c;
		do { ins >> c; } while (c != ':');
		read_value_t<my_value_type>::read(value_, ins);
	}

private:
	_Tv value_;
};

template<typename _Tv>
class vector_t : public setting_t // sequence of zero, one or most setting_t item with same type
{
	static_assert(
		std::is_base_of<setting_t, _Tv>::value ||
		std::is_integral<_Tv>::value ||
		std::is_floating_point<_Tv>::value ||
		std::is_same<_Tv, std::string>::value,
		"Invalid Type in Setting Vector!");

	template<typename _Ty, bool is_setting> struct type_wrapper;
	template<typename _Ty> struct type_wrapper<_Ty, true> { using type = _Ty; };
	template<typename _Ty> struct type_wrapper<_Ty, false> { using type = scalar_t<_Ty>; };
	template<typename _Ty> using type_wrapper_t = typename type_wrapper<_Ty, std::is_base_of<setting_t, _Ty>::value>::type;

public:
	using my_value_type = _Tv;
	using my_type = scalar_t<_Tv>;

	vector_t(std::string _label = "") : setting_t(_label) { vector_.clear(); }
	vector_t(std::string _label, int _size) : setting_t(_label) { resize(_size); }

	const type_t value_type() const
	{
		return type_t::vt_vector;
	}

	const my_value_type & operator()(int index) const
	{
		return (const my_value_type &)vector_[index];
	}

	my_value_type & operator()(int index)
	{
		return vector_[index];
	}

	std::size_t size() const
	{
		return vector_.size();
	}

	void clear(const my_value_type & value)
	{
		vector_.clear();
	}

	void resize(std::size_t _size)
	{
		char buf[50];

		vector_.resize(_size);
		for (int i = 0; i < _size; ++i)
		{
			sprintf(buf, "%s[%2d]", label().c_str(), i + 1);
			vector_[i].label() = buf;
		}
	}

	void scan(bool suggest_current_value = false, int tab_level = 0)
	{
		auto pfx = std::string(tab_level * 2, ' ');

		do
		{
			if (suggest_current_value)
			{
				char ind; // indicator
				do {
					std::cout << pfx << label() << ": " << std::endl;
					std::cout << pfx << "[ Count: " << size() << std::endl;
					for (int i = 0; i < size(); ++i)
					{
						vector_[i].print(std::cout, tab_level + (size() > 1 ? 1 : 0));
					}
					std::cout << pfx << "] OK? (Yes: Press Enter|Edit: 'e' + Enter) " << std::flush;

					do { char ch; while (std::cin.readsome(&ch, 1) != 0); } while (0); // FLUSH_OUT();
					ind = tolower(getchar());
					do { char ch; while (std::cin.readsome(&ch, 1) != 0); } while (0); // FLUSH_OUT();
				} while ((ind != 10 /* Enter -> OK */) && (ind != 'e' /* -> Edit */));

				if (ind == 10 /* Enter -> OK */)
				{
					return;
				}
			}

			std::size_t old_size{ size() }, new_size;

			std::cout << pfx << label() << ": " << std::endl;
			std::cout << pfx << "[ Count? " << std::flush;

			std::cin >> new_size;
			resize(new_size);

			for (int i = 0; i < size(); ++i)
			{
				vector_[i].scan((i < old_size) && suggest_current_value, tab_level + (size() > 1 ? 1 : 0));
			}

			std::cout << pfx << "] " << std::endl << std::flush;
			suggest_current_value = true;
		} while (true);
	}

	void print(std::ostream & ous = std::cout, int tab_level = 0) const
	{
		auto pfx = std::string(tab_level * 2, ' ');

		ous << pfx << label() << ": " << std::endl;
		ous << pfx << "[ Count: " << size() << std::endl;
		for (int i = 0; i < size(); ++i)
		{
			vector_[i].print(ous, tab_level + (size() > 1 ? 1 : 0));
		}
		ous << pfx << "] " << std::endl << std::flush;
	}

	void fread(std::istream & ins)
	{
		int new_size;

		char c;
		do { ins >> c; } while (c != ':');
		do { ins >> c; } while (c != ':');

		ins >> new_size;
		resize(new_size);

		for (int i = 0; i < size(); ++i)
		{
			vector_[i].fread(ins);
		}
	}

protected:
	std::vector<type_wrapper_t<my_value_type>> vector_;
};

class group_t : public setting_t // <virtual class> sequence of zero, one or most setting_t item with different type
{
public:
	group_t(const std::string & _label = "") : setting_t(_label) {  }

	const type_t value_type() const
	{
		return type_t::vt_group;
	}

	virtual std::size_t size() const = 0;

	virtual const setting_t & operator()(int index) const = 0;

	virtual setting_t & operator()(int index)
	{
		return const_cast<setting_t &>((static_cast<const group_t&>(*this)).operator()(index));
	}

	void scan(bool suggest_current_value = false, int tab_level = 0)
	{
		auto pfx = std::string(tab_level * 2, ' ');

		do
		{
			if (suggest_current_value)
			{
				char ind; // indicator
				do {
					std::cout << pfx << label() << ": " << std::endl;
					std::cout << pfx << "{ " << std::endl;
					for (int i = 0; i < size(); ++i)
					{
						operator()(i).print(std::cout, tab_level + (size() > 1 ? 1 : 0));
					}
					std::cout << pfx << "} OK? (Yes: Press Enter|Edit: 'e' + Enter) " << std::flush;

					do { char ch; while (std::cin.readsome(&ch, 1) != 0); } while (0); // FLUSH_OUT();
					ind = tolower(getchar());
					do { char ch; while (std::cin.readsome(&ch, 1) != 0); } while (0); // FLUSH_OUT();
				} while ((ind != 10 /* Enter -> OK */) && (ind != 'e' /* -> Edit */));

				if (ind == 10 /* Enter -> OK */)
				{
					return;
				}
			}

			std::cout << pfx << label() << ": " << std::endl;
			std::cout << pfx << "{ " << std::endl;

			for (int i = 0; i < size(); ++i)
			{
				operator()(i).scan(suggest_current_value, tab_level + (size() > 1 ? 1 : 0));
			}

			std::cout << pfx << "} " << std::endl << std::flush;
			suggest_current_value = true;
		} while (true);
	}

	void print(std::ostream & ous = std::cout, int tab_level = 0) const
	{
		auto pfx = std::string(tab_level * 2, ' ');

		ous << pfx << label() << ": " << std::endl;
		ous << pfx << "{ " << std::endl;
		for (int i = 0; i < size(); ++i)
		{
			operator()(i).print(ous, tab_level + (size() > 1 ? 1 : 0));
		}
		ous << pfx << "} " << std::endl << std::flush;
	}

	void fread(std::istream & ins)
	{
		char c;
		do { ins >> c; } while (c != ':');
		do { ins >> c; } while (c != '{');
		for (int i = 0; i < size(); ++i)
		{
			operator()(i).fread(ins);
		}
	}
};

#endif // !_SETTING_T_HPP_

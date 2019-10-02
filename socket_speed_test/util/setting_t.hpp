#ifndef _SETTING_T_HPP_
#define _SETTING_T_HPP_

#include <exception>
#include <algorithm>
#include <assert.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>

class setting_t // virtual class
{
public:
	virtual ~setting_t() = default;
	enum class type_t : int32_t
	{
		vt_none = 0,
		// scalar types -> integral, floating point or string
		vt_scalar,
		// aggregate types -> vector or group
		vt_vector, // sequence of zero, one or most setting_t item with same type
		vt_group // sequence of zero, one or most setting_t item with different type
	};

	virtual type_t value_type() const = 0;
	inline const std::string & label() const { return label_; }
	inline std::string & label() { return label_; }

	inline bool enable() const
	{
		if (enable_callback_ == nullptr)
		{
			return true;
		}

		return enable_callback_(enable_callback_param_);
	}

	inline void set_enable_callback(bool(*_enable_callback)(void*), void * _enable_callback_param = nullptr)
	{
		enable_callback_param_ = _enable_callback_param;
		enable_callback_ = _enable_callback;
	}

	virtual void scan(bool suggest_current_value = false, int32_t tab_level = 0, bool confirm = true) = 0;
	virtual void print(std::ostream & ous = std::cout, int32_t tab_level = 0) const = 0;
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
	void * enable_callback_param_{ nullptr };
	bool(*enable_callback_)(void*) { nullptr };

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
class scalar_t : public setting_t // bool, int32_t, int64, double, float or string
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

	type_t value_type() const
	{
		return type_t::vt_scalar;
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

	void scan(bool suggest_current_value = false, int32_t tab_level = 0, bool confirm = false /* unused */)
	{
		if (!enable())
		{
			return;
		}

		auto pfx = std::string((std::size_t)(tab_level * 2), ' ');

		do
		{
			if (suggest_current_value)
			{
				char ind; // indicator
				do {
					std::cout << pfx << label() << ": " << value_ << "; OK? (Yes: Press Enter|Edit: 'e' + Enter) " << std::flush;

					do { char ch; while (std::cin.readsome(&ch, 1) != 0); } while (0); // FLUSH_OUT();
					ind = (char)tolower(getchar());
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

			if (confirm)
			{
				suggest_current_value = true;
			}
			else
			{
				return;
			}
		} while (true);
	}

	void print(std::ostream & ous = std::cout, int32_t tab_level = 0) const
	{
		if (!enable())
		{
			return;
		}

		ous << std::string(tab_level * 2, ' ') << label() << ": " << value_ << std::endl << std::flush;
	}

	void fread(std::istream & ins)
	{
		if (!enable())
		{
			return;
		}

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
	vector_t(std::string _label, int32_t _size) : setting_t(_label) { resize(_size); }

	type_t value_type() const
	{
		return type_t::vt_vector;
	}

	const my_value_type & operator()(std::size_t index) const
	{
		return (const my_value_type&)vector_[index];
	}

	my_value_type & operator()(std::size_t index)
	{
		return vector_[index];
	}

	std::size_t size() const
	{
		return vector_.size();
	}

	void clear()
	{
		vector_.clear();
	}

	void resize(std::size_t _size)
	{
		char buf[50];

		vector_.resize(_size);
		for (std::size_t i = 0; i < _size; ++i)
		{
			sprintf(buf, "%s[%2llu]", label().c_str(), i + 1);
			vector_[i].label() = buf;
		}
	}

	void scan(bool suggest_current_value = false, int32_t tab_level = 0, bool confirm = true)
	{
		if (!enable())
		{
			return;
		}

		auto pfx = std::string((std::size_t)(tab_level * 2), ' ');

		do
		{
			if (suggest_current_value)
			{
				char ind; // indicator
				do {
					std::cout << pfx << label() << ": " << std::endl;
					std::cout << pfx << "[ Count: " << size() << std::endl;
					for (int32_t i = 0; i < size(); ++i)
					{
						vector_[i].print(std::cout, tab_level + (size() > 1 ? 1 : 0));
					}
					std::cout << pfx << "] OK? (Yes: Press Enter|Edit: 'e' + Enter) " << std::flush;

					do { char ch; while (std::cin.readsome(&ch, 1) != 0); } while (0); // FLUSH_OUT();
					ind = (char)tolower(getchar());
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

			for (int32_t i = 0; i < size(); ++i)
			{
				vector_[i].scan((i < old_size) && suggest_current_value, tab_level + (size() > 1 ? 1 : 0), 
					(size() > 1) && (vector_[i].value_type() != type_t::vt_scalar));
			}

			std::cout << pfx << "] " << std::endl << std::flush;

			if (confirm)
			{
				suggest_current_value = true;

				for (int32_t i = 0; i < 8; ++i)
				{
					std::cout << "########";
				}
				std::cout << std::endl << std::flush;
			}
			else
			{
				return;
			}
		} while (true);
	}

	void print(std::ostream & ous = std::cout, int32_t tab_level = 0) const
	{
		if (!enable())
		{
			return;
		}

		auto pfx = std::string((std::size_t)(tab_level * 2), ' ');

		ous << pfx << label() << ": " << std::endl;
		ous << pfx << "[ Count: " << size() << std::endl;
		for (int32_t i = 0; i < size(); ++i)
		{
			vector_[i].print(ous, tab_level + (size() > 1 ? 1 : 0));
		}
		ous << pfx << "] " << std::endl << std::flush;
	}

	void fread(std::istream & ins)
	{
		if (!enable())
		{
			return;
		}

		int32_t new_size;

		char c;
		do { ins >> c; } while (c != ':');
		do { ins >> c; } while (c != ':');

		ins >> new_size;
		resize(new_size);

		for (int32_t i = 0; i < size(); ++i)
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

	type_t value_type() const
	{
		return type_t::vt_group;
	}

	virtual std::size_t size() const = 0;

	virtual const setting_t & operator()(std::size_t index) const = 0;

	virtual setting_t & operator()(std::size_t index)
	{
		return const_cast<setting_t &>((static_cast<const group_t&>(*this)).operator()(index));
	}

	void scan(bool suggest_current_value = false, int32_t tab_level = 0, bool confirm = true)
	{
		if (!enable())
		{
			return;
		}

		auto pfx = std::string((std::size_t)(tab_level * 2), ' ');

		do
		{
			if (suggest_current_value)
			{
				char ind; // indicator
				do {
					std::cout << pfx << label() << ": " << std::endl;
					std::cout << pfx << "{ " << std::endl;
					for (std::size_t i = 0; i < size(); ++i)
					{
						operator()(i).print(std::cout, tab_level + (size() > 1 ? 1 : 0));
					}
					std::cout << pfx << "} OK? (Yes: Press Enter|Edit: 'e' + Enter) " << std::flush;

					do { char ch; while (std::cin.readsome(&ch, 1) != 0); } while (0); // FLUSH_OUT();
					ind = (char)tolower(getchar());
					do { char ch; while (std::cin.readsome(&ch, 1) != 0); } while (0); // FLUSH_OUT();
				} while ((ind != 10 /* Enter -> OK */) && (ind != 'e' /* -> Edit */));

				if (ind == 10 /* Enter -> OK */)
				{
					return;
				}
			}

			std::cout << pfx << label() << ": " << std::endl;
			std::cout << pfx << "{ " << std::endl;

			for (std::size_t i = 0; i < size(); ++i)
			{
				operator()(i).scan(suggest_current_value, tab_level + (size() > 1 ? 1 : 0), 
					(size() > 1) && (operator()(i).value_type() != type_t::vt_scalar));
			}

			std::cout << pfx << "} " << std::endl << std::flush;

			if (confirm)
			{
				suggest_current_value = true;

				for (int32_t i = 0; i < 8; ++i)
				{
					std::cout << "########";
				}
				std::cout << std::endl << std::flush;
			}
			else
			{
				return;
			}
		} while (true);
	}

	void print(std::ostream & ous = std::cout, int32_t tab_level = 0) const
	{
		if (!enable())
		{
			return;
		}

		auto pfx = std::string((std::size_t)(tab_level * 2), ' ');

		ous << pfx << label() << ": " << std::endl;
		ous << pfx << "{ " << std::endl;
		for (std::size_t i = 0; i < size(); ++i)
		{
			operator()(i).print(ous, tab_level + (size() > 1 ? 1 : 0));
		}
		ous << pfx << "} " << std::endl << std::flush;
	}

	void fread(std::istream & ins)
	{
		if (!enable())
		{
			return;
		}

		char c;
		do { ins >> c; } while (c != ':');
		do { ins >> c; } while (c != '{');
		for (std::size_t i = 0; i < size(); ++i)
		{
			operator()(i).fread(ins);
		}
	}
};

#endif // !_SETTING_T_HPP_

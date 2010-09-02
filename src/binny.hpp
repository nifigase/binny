/*
 * binny.hpp
 *
 *  Created on: Aug 18, 2010
 *      Author: andrew
 *  For usage example see test.cpp
 */

#ifndef BINNY_HPP_
#define BINNY_HPP_

#include <iostream>
#include <stdexcept>
#include <iterator>
#include <sstream>
#include <limits.h>

#include "boost/static_assert.hpp"
#include "boost/concept_check.hpp"
#include "boost/concept_archetype.hpp"

#define BINNY_BITSZ(x) (sizeof(x) * CHAR_BIT)

namespace binny {

class bin_match_error : public std::logic_error {
public:
	static bin_match_error create(unsigned int el_num, unsigned int bit_num, int expected, int actual) {
		std::ostringstream ss;
		ss << "Parse error at buffer element " << el_num << ", bit " << bit_num
				<< ": expected " << expected << ", given " << actual;
		return bin_match_error(ss.str());
	}
private:
	explicit bin_match_error(const std::string& err) : std::logic_error(err) {}
};

template <typename BinT>
struct BinDataConcept {
    void constraints() {
		//support context
        typename BinT::context_t context = bin_.new_context();

        char ch; //buffer
        try {
        	bin_.consume(ch, 0, 3, context);
        } catch (const bin_match_error& e) {}
    }
    BinT bin_;
	typename BinT::storage_t storage_;
};


template<typename ValueT>
class bin_data_base {
private:
	const unsigned int bits_;
public:
	typedef ValueT storage_t;
	inline unsigned int bits() const { return bits_; }

	bin_data_base(const bin_data_base<ValueT>& rhs) :
		bits_(rhs.bits_) {}

	struct context {
		int consumed;
		context() : consumed(0) {}
	};
	typedef context context_t;

	context_t new_context() const { return context_t(); }

protected:
	explicit bin_data_base(unsigned int bits) :
		bits_(bits) {}

	template<typename BufferT>
	unsigned int calculate_to_consume(BufferT el, unsigned int bit_from, context_t& ctx) const {
		return std::min(BINNY_BITSZ(BufferT) - bit_from, bits() - ctx.consumed);
	}

	/**
	 * Note, that bit_from counts from left, val_bit_from - from right
	 */
	template<typename BufferT>
	void validate_one(BufferT el,
			unsigned int el_num, unsigned int bit_from,
			const storage_t& val, unsigned int val_bit_from, unsigned int to_consume) const {

		BufferT test_el = 0;
		storage_t test_val = 0;
		BufferT mask_el = (1 << (BINNY_BITSZ(BufferT) - bit_from - 1));
		storage_t mask_val = (1 << val_bit_from);
		for (unsigned int i = 0; i < to_consume;
				++i, mask_el = mask_el >> 1, mask_val = mask_val >> 1
		) {
			test_el = el & mask_el;
			test_val = val & mask_val;
			if ((test_el != 0 && test_val == 0) || (test_el == 0 && test_val != 0)) {
				//different bits
				throw bin_match_error::create(el_num, bit_from + i,
						test_val == 0 ? 0 : 1, //expected
						test_el  == 0 ? 0 : 1  //actual
				);
			}
		}
	}

	/**
	 * Note, that bit_from counts from left, val_bit_from - from right
	 */
	template<typename BufferT>
	void fill_one(BufferT el,
			unsigned int el_num, unsigned int bit_from,
			storage_t& val, unsigned int val_bit_from, unsigned int to_consume) const {

		BufferT mask_el = (1 << (BINNY_BITSZ(BufferT) - bit_from - 1));
		storage_t mask_val = (1 << val_bit_from);
		for (unsigned int i = 0; i < to_consume;
				++i, mask_el = mask_el >> 1, mask_val = mask_val >> 1
		) {
			if (el & mask_el)
				val |= mask_val;
			else
				val &= ~mask_val;
		}
	}
};

template<typename ValueT>
class const_data_base : public bin_data_base<ValueT> {
private:
	const ValueT value_;
	typedef bin_data_base<ValueT> parent_t;
protected:
	const_data_base(const ValueT value, unsigned int bits) : parent_t(bits), value_(value) {}

public:
	/**
	 * match few bits against those in value_
	 */
	template<typename BufferT>
	unsigned int consume(BufferT el,
			unsigned int el_num, unsigned int bit_from, typename parent_t::context_t& ctx) const {

		unsigned int to_consume = calculate_to_consume(el, bit_from, ctx);
		unsigned int val_bit_from = parent_t::bits() - ctx.consumed - 1;
		validate_one(el, el_num, bit_from, value_, val_bit_from, to_consume);
		ctx.consumed += to_consume;
		return to_consume;
	}
};

template<typename IterT>
class const_data_array : public bin_data_base<typename std::iterator_traits<IterT>::value_type> {
private:
	const IterT begin_;
	const IterT end_;
	typedef bin_data_base<typename std::iterator_traits<IterT>::value_type> parent_t;

	struct context : public parent_t::context_t {
		IterT cur;
		context(const IterT it) : cur(it) {}
	};
	typedef context context_t;

	context_t new_context() const {
		return context_t(begin_);
	}

public:
	const_data_array(const IterT begin, const IterT end) :
		parent_t(std::distance(begin, end) * BINNY_BITSZ(parent_t::storage_t)),
		begin_(begin), end_(end) {}

	const_data_array(const IterT begin, const IterT end, unsigned int bits) :
		parent_t(std::min(std::distance(begin, end) * BINNY_BITSZ(parent_t::storage_t), bits)),
		begin_(begin), end_(end) {}

	/**
	 * match few bits against those in some elements in value_
	 */
	template<typename BufferT>
	unsigned int consume(BufferT el, unsigned int el_num, unsigned int bit_from, context_t& ctx) const {

		const unsigned int consumed_before = ctx.consumed;
		while (ctx.cur != end_) {
			const bool last_el = parent_t::bits() - ctx.consumed <= BINNY_BITSZ(parent_t::storage_t);
			const unsigned int val_bit_from = last_el ?
					parent_t::bits() - ctx.consumed - 1 :
					BINNY_BITSZ(parent_t::storage_t) - (ctx.consumed % BINNY_BITSZ(parent_t::storage_t)) -1;
			const unsigned int to_consume = std::min(calculate_to_consume(el, bit_from), val_bit_from);

			validate_one(el, el_num, bit_from, *ctx.cur, val_bit_from, to_consume);
			ctx.consumed += to_consume;

			if (ctx.consumed % BINNY_BITSZ(parent_t::storage_t) == 0) ++ctx.cur;

			bit_from += to_consume;
			if (bit_from >= BINNY_BITSZ(BufferT)) break;
		}
		return ctx.consumed - consumed_before;
	}
};

/**
 * constant bin container. Usage: c_bin_data<100110>
 */
template<unsigned long BIN>
class c_bin_data : public const_data_base<unsigned long> {
public:
	static const unsigned long value = BIN % 10 + 2 * c_bin_data<BIN/10>::value;
	static const unsigned int length = 1 + c_bin_data<BIN/10>::length;

	c_bin_data() : const_data_base<unsigned long>(value, length) {}
	BOOST_STATIC_ASSERT((BIN % 10 < 2));
};

template<>
class c_bin_data<0ul> : public const_data_base<unsigned long> {
public:
	static const unsigned long value = 0ul;
	static const unsigned int length = 1;

	c_bin_data() : const_data_base<unsigned long>(value, length) {}
};

template<>
class c_bin_data<1ul> : public const_data_base<unsigned long> {
public:
	static const unsigned long value = 1ul;
	static const unsigned int length = 1;

	c_bin_data() : const_data_base<unsigned long>(value, length) {}
};

template<unsigned int NUM>
class zeros : public const_data_base<unsigned long> {
public:
	static const unsigned long value = 0ul;
	static const unsigned int length = NUM;

	zeros() : const_data_base<unsigned long>(value, length) {}
};

template<typename ValueT>
class nonconst_data : public bin_data_base<ValueT> {
private:
	ValueT& value_;
	typedef bin_data_base<ValueT> parent_t;

public:
	explicit nonconst_data(ValueT& value, unsigned int bits = static_cast<unsigned int>(BINNY_BITSZ(ValueT))) :
		parent_t(bits),
		value_(value) {}

	/**
	 * fill few bits with those in value_
	 */
	template<typename BufferT>
	unsigned int consume(BufferT el, unsigned int el_num, unsigned int bit_from, typename parent_t::context_t& ctx) {

		unsigned int to_consume = calculate_to_consume(el, bit_from, ctx);
		unsigned int val_bit_from = parent_t::bits() - ctx.consumed - 1;
		fill_one(el, el_num, bit_from, value_, val_bit_from, to_consume);
		ctx.consumed += to_consume;
		return to_consume;
	}
};

//generators

template<typename ValueT>
nonconst_data<ValueT> nonconst(ValueT& v, unsigned int bits = static_cast<unsigned int>(BINNY_BITSZ(ValueT))) {
	return nonconst_data<ValueT>(v, bits);
}

//holder traits

template<typename HolderT>
struct holder_traits {
	typedef const HolderT& member_type;
};
template<typename ValueT>
struct holder_traits <nonconst_data<ValueT> > {
	typedef nonconst_data<ValueT> member_type;
};

/**
 * Pair of holders. Holders compose a tree:
 * bin_holder_sequence
 *   |             |
 *   |			BinHolder2
 *   |
 * bin_holder_sequence
 *   |             |
 *   |			BinHolder2
 *   |
 * ...
 * bin_holder_sequence
 *   |             |
 * BinHolder1	BinHolder2
 */
template<typename BinHolder1, typename BinHolder2>
class bin_holder_sequence {
private:
	typename holder_traits<BinHolder1>::member_type holder1_;
	typename holder_traits<BinHolder2>::member_type holder2_;
public:
	bin_holder_sequence(const BinHolder1& holder1, const BinHolder2& holder2) :
		holder1_(holder1), holder2_(holder2) {}

	inline typename holder_traits<BinHolder1>::member_type holder1() const { return holder1_; }
	inline typename holder_traits<BinHolder2>::member_type holder2() const { return holder2_; }
};

/// all sequence operators

#define BINNY_CONSTRUCT_SEQ_OPERATOR(TEMPLATE1, TEMPLATE2) \
	template<typename T1, typename T2> \
	bin_holder_sequence<TEMPLATE1<T1>, TEMPLATE2<T2> > \
	operator|(const TEMPLATE1<T1>& d1, const TEMPLATE2<T2>& d2) { \
		return bin_holder_sequence<TEMPLATE1<T1>, TEMPLATE2<T2> >(d1, d2); \
	}
#define BINNY_CONSTRUCT_SEQ_OPERATOR_SEQ(TEMPLATE2) \
	template<typename H1, typename H2, typename T2> \
	bin_holder_sequence<bin_holder_sequence<H1, H2>, TEMPLATE2<T2> > \
	operator|(const bin_holder_sequence<H1, H2>& d1, const TEMPLATE2<T2>& d2) { \
		return bin_holder_sequence<bin_holder_sequence<H1, H2>, TEMPLATE2<T2> >(d1, d2); \
	}

///TODO replace with BOOST PP invocations
BINNY_CONSTRUCT_SEQ_OPERATOR(nonconst_data, nonconst_data)
BINNY_CONSTRUCT_SEQ_OPERATOR(nonconst_data, const_data_base)
BINNY_CONSTRUCT_SEQ_OPERATOR(nonconst_data, const_data_array)

BINNY_CONSTRUCT_SEQ_OPERATOR(const_data_base, nonconst_data)
BINNY_CONSTRUCT_SEQ_OPERATOR(const_data_base, const_data_base)
BINNY_CONSTRUCT_SEQ_OPERATOR(const_data_base, const_data_array)

BINNY_CONSTRUCT_SEQ_OPERATOR(const_data_array, nonconst_data)
BINNY_CONSTRUCT_SEQ_OPERATOR(const_data_array, const_data_base)
BINNY_CONSTRUCT_SEQ_OPERATOR(const_data_array, const_data_array)

BINNY_CONSTRUCT_SEQ_OPERATOR_SEQ(nonconst_data)
BINNY_CONSTRUCT_SEQ_OPERATOR_SEQ(const_data_base)
BINNY_CONSTRUCT_SEQ_OPERATOR_SEQ(const_data_array)

//TODO nonconst_data_array


template<typename SourceIterT, typename BinHolderT>
void match(SourceIterT& src_it,
		unsigned int& el_num, unsigned int& bit_from, BinHolderT holder) {

	BOOST_CLASS_REQUIRE(BinHolderT, binny, BinDataConcept);

	typedef typename std::iterator_traits<SourceIterT>::value_type BufferT;

	typename BinHolderT::context_t ctx = holder.new_context();

	for (unsigned int bits_to_read = holder.bits(); bits_to_read; ) {

		BufferT cur = *src_it;
		unsigned int consumed = holder.consume(cur, el_num, bit_from, ctx);
		bits_to_read -= consumed;
		bit_from += consumed;
		if (bit_from >= BINNY_BITSZ(BufferT)) {
			bit_from = 0;
			++src_it;
			++el_num;
		}
	}
}

template<typename SourceIterT, typename BinHolder1, typename BinHolder2>
void match(SourceIterT& src_it,
		unsigned int& el_num, unsigned int& bit_from,
		const bin_holder_sequence<BinHolder1, BinHolder2>& seq) {
	match(src_it, el_num, bit_from, seq.holder1());
	match(src_it, el_num, bit_from, seq.holder2());
}


/**
 * parse func
 * @throw bin_match_error
 */
template <typename BinHolderT, typename SourceIterT>
void parse(const BinHolderT& holder, SourceIterT src_it) {
	unsigned int el_num = 0;
	unsigned int bit_from = 0;
	match(src_it, el_num, bit_from, holder);
}

} //namespace

#endif /* BINNY_HPP_ */

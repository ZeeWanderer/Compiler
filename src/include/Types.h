#pragma once

#include <map>
#include <set>
#include <memory>

#include "llvm/IR/Type.h"
#include "llvm/IR/Instructions.h"

namespace slljit
{
	using namespace std;
	using namespace llvm;

	class LocalContext;

	//! TypeID enum.
	/*! Represents value types. */
	enum TypeID
	{
		boolTyID,   /*!< boolean int1 unsigned */
		doubleTyID, /*!< double 64 bit wide */
		int64TyID,  /*!< signed int64 */
		uint64TyID, /*!< unsigned int64 */

		functionTyID, /*!< Represents function */
		none          /*!< untyped value. Not synonymous to void. */
	};

	namespace
	{
		const set<string> basic_types_identifier_set = {
		    "double",
		    "int64",
		    "uint64",
		    "bool"};

		const std::map<std::string, TypeID> basic_types_id_map = {
		    {"double", doubleTyID},
		    {"int64", int64TyID},
		    {"uint64", uint64TyID},
		    {"bool", boolTyID}};

		const map<set<TypeID>, TypeID> implict_cast_lookup = {
		    //any iperation with floating point results in floating point
		    {{doubleTyID, int64TyID}, doubleTyID},
		    {{doubleTyID, uint64TyID}, doubleTyID},
		    {{doubleTyID, boolTyID}, doubleTyID},

		    // operation with signed integer results in signed integer
		    {{int64TyID, uint64TyID}, int64TyID},
		    {{int64TyID, boolTyID}, int64TyID},

		    // if both integer types have the same signedness cast to wider
		    {{uint64TyID, boolTyID}, uint64TyID}};

		const map<TypeID, llvm::Type::TypeID> inbuilt_to_llvm_lookup = {
		    {doubleTyID, Type::DoubleTyID},
		    {int64TyID, Type::IntegerTyID},
		    {uint64TyID, Type::IntegerTyID},
		    {boolTyID, Type::IntegerTyID}};

		// floating point <-> int cast operations lookup table
		// (from, to)
		const map<std::pair<TypeID, TypeID>, llvm::Instruction::CastOps> fp_int_cast_op_lookup = {
		    // double <-> int64
		    {{doubleTyID, int64TyID}, Instruction::FPToSI},
		    {{int64TyID, doubleTyID}, Instruction::SIToFP},

		    // double <-> uint64
		    {{doubleTyID, uint64TyID}, Instruction::FPToUI},
		    {{uint64TyID, doubleTyID}, Instruction::UIToFP},

		    // double <-> bool
		    {{doubleTyID, boolTyID}, Instruction::FPToUI},
		    {{boolTyID, doubleTyID}, Instruction::UIToFP}};
	}; // namespace

	/// Translates TypeID types to llvm types.
	llvm::Type* get_llvm_type(TypeID TyID, LocalContext& m_local_context);

	/**
	 * Used for implict casting.
	 * @return Resulting type for any operation perormed on given types.
	 */
	TypeID operator+(TypeID lhs, TypeID rhs);

	/// Checks if value type is floating point type.
	bool isFloatingPointTy(TypeID type_);

	/// Checks if value type is integer type.
	bool isIntegerTy(TypeID type_);

	/**
	 * Checks if value type is signed.
	 * Requires type to be integer.
	 */
	bool isSigned(TypeID type_);

}; // namespace slljit
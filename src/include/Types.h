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

	enum TypeID
	{
		doubleTyID,
		int64TyID,
		uint64TyID,

		// special types
		functionTyID,
		none
	};

	namespace
	{
		const set<string> basic_types_identifier_set = {
		    "double",
		    "int64"};

		const std::map<std::string, TypeID> basic_types_id_map = {
		    {"double", doubleTyID},
		    {"int64", int64TyID}};

		const map<set<TypeID>, TypeID> implict_cast_loockup = {
		    //any iperation with floating point results in floating point
		    {{doubleTyID, int64TyID}, doubleTyID},
		    {{doubleTyID, uint64TyID}, doubleTyID},

		    // operation with signed integer results in signed integer
		    {{uint64TyID, int64TyID}, int64TyID}};

		const map<TypeID, llvm::Type::TypeID> inbuilt_to_llvm_lookup = {
		    {doubleTyID, Type::DoubleTyID},
		    {int64TyID, Type::IntegerTyID},
		    {uint64TyID, Type::IntegerTyID}};

		// (from, to)
		const map<std::pair<TypeID, TypeID>, llvm::Instruction::CastOps> cast_op_lookup = {
		    {{doubleTyID, int64TyID}, Instruction::FPToSI},
		    {{int64TyID, doubleTyID}, Instruction::SIToFP},

		    {{doubleTyID, uint64TyID}, Instruction::FPToUI},
		    {{uint64TyID, doubleTyID}, Instruction::UIToFP},

		    {{int64TyID, uint64TyID}, Instruction::ZExt},
		    {{uint64TyID, int64TyID}, Instruction::ZExt}};

	}; // namespace

	llvm::Type* get_llvm_type(TypeID TyID, LocalContext& m_local_context);

	TypeID operator+(TypeID lhs, TypeID rhs);

}; // namespace slljit
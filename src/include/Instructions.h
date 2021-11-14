#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"

#include "Types.h"

#include <map>

namespace slljit
{
	using namespace std;
	using namespace llvm;

	class LocalContext;

	enum CmpPredicate
	{
		equal,
		not_equal,
		greater_than,
		greater_or_equal,
		less_then,
		less_or_equal,
	};

	namespace
	{
		const map<CmpPredicate, llvm::CmpInst::Predicate> llvm_fp_pred = {
		    {equal, CmpInst::Predicate::FCMP_UEQ},
		    {not_equal, CmpInst::Predicate::FCMP_UNE},
		    {greater_than, CmpInst::Predicate::FCMP_UGT},
		    {greater_or_equal, CmpInst::Predicate::FCMP_UGE},
		    {less_then, CmpInst::Predicate::FCMP_ULT},
		    {less_or_equal, CmpInst::Predicate::FCMP_ULE}};

		const map<CmpPredicate, llvm::CmpInst::Predicate> llvm_int_pred = {
		    {equal, CmpInst::Predicate::ICMP_EQ},
		    {not_equal, CmpInst::Predicate::ICMP_NE},
		    {greater_than, CmpInst::Predicate::ICMP_SGT},
		    {greater_or_equal, CmpInst::Predicate::ICMP_SGE},
		    {less_then, CmpInst::Predicate::ICMP_SLT},
		    {less_or_equal, CmpInst::Predicate::ICMP_SLE}};

		const map<TypeID, const map<CmpPredicate, llvm::CmpInst::Predicate>> pred_to_llvm_pred = {
		    {doubleTyID, llvm_fp_pred},
		    {int64TyID, llvm_int_pred},
		    {uint64TyID, llvm_int_pred},
		    {boolTyID, llvm_int_pred}};
	}; // namespace

	void CreateExplictCast(Value*& val, TypeID fromTy, TypeID toTy, LocalContext& ctx);

	TypeID CreateImplictCast(Value*& lhs, Value*& rhs, TypeID lhsTy, TypeID rhsTy, LocalContext& ctx);

	// Requires both operands to be of the same type
	Value* CtreateCMP(CmpPredicate pred, Value* lhs, Value* rhs, TypeID type_, LocalContext& ctx, const llvm::Twine& Name = "");

	// Requires both operands to be of the same type
	Value* CreateAdd(Value*& lhs, Value*& rhs, TypeID type_, LocalContext& ctx, const llvm::Twine& Name = "");

	// Requires both operands to be of the same type
	Value* CreateSub(Value*& lhs, Value*& rhs, TypeID type_, LocalContext& ctx, const llvm::Twine& Name = "");

	// Requires both operands to be of the same type
	Value* CreateMul(Value*& lhs, Value*& rhs, TypeID type_, LocalContext& ctx, const llvm::Twine& Name = "");

	// Requires both operands to be of the same type
	Value* CreateDiv(Value*& lhs, Value*& rhs, TypeID type_, LocalContext& ctx, const llvm::Twine& Name = "");

}; // namespace slljit
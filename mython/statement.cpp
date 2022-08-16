#include "statement.h"

#include <iostream>
#include <sstream>

using namespace std;

namespace ast {

	using runtime::Closure;
	using runtime::Context;
	using runtime::ObjectHolder;

	namespace {
		const string ADD_METHOD = "__add__"s;
		const string INIT_METHOD = "__init__"s;
		const string STR_METHOD = "__str__"s;
	}  // namespace

	ObjectHolder Assignment::Execute(Closure& closure, Context& context ) {		
		return closure[var_] = rv_.get()->Execute(closure, context);		
	}

	Assignment::Assignment(std::string var, std::unique_ptr<Statement> rv)
		: var_(var), rv_(std::move(rv)) {
	}

	VariableValue::VariableValue(const std::string& var_name)
		: ids_{ var_name } {
	}

	VariableValue::VariableValue(std::vector<std::string> dotted_ids)
		: ids_(dotted_ids) {		
	}

	ObjectHolder VariableValue::Execute(Closure& closure, Context& context) {
		if (closure.count(ids_[0]) == 0) {
			throw runtime_error("No such variable!");
		}
		
		auto& result = closure.at(ids_[0]);
		if (ids_.size() > 1) {		
			auto obj = result.TryAs<runtime::ClassInstance>();
			return VariableValue( std::vector<std::string>{ ids_.begin() + 1, ids_.end() } ).Execute(obj->Fields(), context);
		}

		return result;
	}

	unique_ptr<Print> Print::Variable(const std::string& name) {
		
		return std::move(make_unique<Print>(make_unique<VariableValue>(name)));
	}

	Print::Print(unique_ptr<Statement> argument) {
		args_.push_back(std::move(argument));
	}

	Print::Print(vector<unique_ptr<Statement>> args)
		: args_(std::move(args)) {
	}

	ObjectHolder Print::Execute(Closure& closure, Context& context) {		
		auto& out = context.GetOutputStream();
				
		for (const auto& arg : args_) {
			const auto temp_exec = arg->Execute(closure, context);
			if (temp_exec) {
				temp_exec->Print(out, context);
			}	
			else {
				out << "None";
			}
			// После последнего символа пробел ставить не надо
			if (&args_.back() != &arg) {
				out.put(' ');
			}				
		}
		
		context.GetOutputStream().put('\n');
		return {};
	}

	MethodCall::MethodCall(std::unique_ptr<Statement> object, std::string method,
		std::vector<std::unique_ptr<Statement>> args)
		: object_(std::move(object)), method_(method), args_(std::move(args)) {
	}

	ObjectHolder MethodCall::Execute(Closure& closure, Context& context) {
		vector<ObjectHolder> obj_args;
		for (const auto& arg : args_) {
			obj_args.push_back(arg->Execute(closure, context));
		}

		auto obj = object_->Execute(closure, context);
		return obj.TryAs<runtime::ClassInstance>()->Call(method_, obj_args, context);	
	}

	ObjectHolder Stringify::Execute(Closure& closure, Context& context) {
		
		auto obj = UnaryOperation::argument_->Execute(closure, context);
		stringstream value;
		
		const auto cls_ptr = obj.TryAs<runtime::ClassInstance>();
		if (cls_ptr != nullptr) {
			if (cls_ptr->HasMethod(STR_METHOD, 0)) {
				const auto res = cls_ptr->Call(STR_METHOD, {}, context);
				res->Print(value, context);
			}			
			else {
				value << cls_ptr;
			}
		}
		// Пустой объект
		else if (!obj) {
			value << "None";
		}
		else { 
			obj->Print(value, context);
		}		
		
		return runtime::ObjectHolder::Own(runtime::String(value.str()));
	}

	ObjectHolder Add::Execute(Closure& closure, Context& context) {
		auto lhs_obj = lhs_->Execute(closure, context);
		auto rhs_obj = rhs_->Execute(closure, context);
		// Оба аргумента числа
		if (lhs_obj.TryAs<runtime::Number>() != nullptr && rhs_obj.TryAs<runtime::Number>() != nullptr)	{
			const int result = lhs_obj.TryAs<runtime::Number>()->GetValue() + rhs_obj.TryAs<runtime::Number>()->GetValue();
			return ObjectHolder::Own(runtime::Number{ result });
		}
		else if (lhs_obj.TryAs<runtime::String>() != nullptr && rhs_obj.TryAs<runtime::String>() != nullptr) {
			const std::string result = lhs_obj.TryAs<runtime::String>()->GetValue() + rhs_obj.TryAs<runtime::String>()->GetValue();
			return ObjectHolder::Own(runtime::String{ result });
		}
		else if (lhs_obj.TryAs<runtime::ClassInstance>() != nullptr) {
			auto lhs_class_obj = lhs_obj.TryAs<runtime::ClassInstance>();
			if (lhs_class_obj->HasMethod(ADD_METHOD, 1)) {
				return lhs_class_obj->Call(ADD_METHOD, { rhs_obj }, context);
			}
		}

		throw std::runtime_error("Wrong types!");
	}

	ObjectHolder Sub::Execute(Closure& closure, Context& context) {
		auto lhs_obj = lhs_->Execute(closure, context);
		auto rhs_obj = rhs_->Execute(closure, context);
		// Оба аргумента числа
		if (lhs_obj.TryAs<runtime::Number>() != nullptr && rhs_obj.TryAs<runtime::Number>() != nullptr) {
			const int result = lhs_obj.TryAs<runtime::Number>()->GetValue() - rhs_obj.TryAs<runtime::Number>()->GetValue();
			return ObjectHolder::Own(runtime::Number{ result });
		}

		throw std::runtime_error("Wrong types!");
	}

	ObjectHolder Mult::Execute(Closure& closure, Context& context) {
		auto lhs_obj = lhs_->Execute(closure, context);
		auto rhs_obj = rhs_->Execute(closure, context);
		// Оба аргумента числа
		if (lhs_obj.TryAs<runtime::Number>() != nullptr && rhs_obj.TryAs<runtime::Number>() != nullptr) {
			const int result = lhs_obj.TryAs<runtime::Number>()->GetValue() * rhs_obj.TryAs<runtime::Number>()->GetValue();
			return ObjectHolder::Own(runtime::Number{ result });
		}

		throw std::runtime_error("Wrong types!");
	}

	ObjectHolder Div::Execute(Closure& closure, Context& context) {
		auto lhs_obj = lhs_->Execute(closure, context);
		auto rhs_obj = rhs_->Execute(closure, context);
		// Оба аргумента числа
		if (lhs_obj.TryAs<runtime::Number>() != nullptr && rhs_obj.TryAs<runtime::Number>() != nullptr) {
			if (rhs_obj.TryAs<runtime::Number>()->GetValue() == 0) {
				throw std::runtime_error("Zero division!");
			}
			else {
				const int result = lhs_obj.TryAs<runtime::Number>()->GetValue() / rhs_obj.TryAs<runtime::Number>()->GetValue();
				return ObjectHolder::Own(runtime::Number{ result });
			}
		}
		throw std::runtime_error("Wrong types!");
	}

	ObjectHolder Compound::Execute(Closure& closure, Context& context) {
		for (const auto& stmt : stmts_) {
			stmt->Execute(closure, context);
		}

		return None{}.Execute(closure, context);
	}

	ObjectHolder Return::Execute(Closure& closure, Context& context) {
		throw ReturnException(std::move(statement_->Execute(closure, context)));
	}

	ClassDefinition::ClassDefinition(ObjectHolder cls)
		:  cls_(cls) {
	}

	ObjectHolder ClassDefinition::Execute(Closure& closure, Context& /*context*/) {		
		runtime::ClassInstance new_inst{ *cls_.TryAs<runtime::Class>() };

		return closure[cls_.TryAs<runtime::Class>()->GetName()] = ObjectHolder::Own(std::move(new_inst));		
	}

	FieldAssignment::FieldAssignment(VariableValue object, std::string field_name,
		std::unique_ptr<Statement> rv)
		: object_(object), field_name_(field_name), rv_(std::move(rv)) {
	}

	ObjectHolder FieldAssignment::Execute(Closure& closure, Context& context) {		
		auto cls = object_.Execute(closure, context).TryAs<runtime::ClassInstance>();
		
		return cls->Fields()[field_name_] = rv_->Execute(closure, context);
	}

	IfElse::IfElse(std::unique_ptr<Statement> condition, std::unique_ptr<Statement> if_body, std::unique_ptr<Statement> else_body)
		: condition_(std::move(condition)), if_body_(std::move(if_body)), else_body_(std::move(else_body)) {
	}

	ObjectHolder IfElse::Execute(Closure& closure, Context& context) {
		
		if (runtime::IsTrue(condition_->Execute(closure, context))) {
			return if_body_->Execute(closure, context);
		}
		else if (else_body_ != nullptr) {			
			return else_body_->Execute(closure, context);
			
		}
		return {};
	}

	ObjectHolder Or::Execute(Closure& closure, Context& context) {
		bool result = runtime::IsTrue(lhs_->Execute(closure, context)) || runtime::IsTrue(rhs_->Execute(closure, context));

		return ObjectHolder::Own(runtime::Bool(result));
	}

	ObjectHolder And::Execute(Closure& closure, Context& context) {
		bool result = runtime::IsTrue(lhs_->Execute(closure, context)) && runtime::IsTrue(rhs_->Execute(closure, context));

		return ObjectHolder::Own(runtime::Bool(result));
	}

	ObjectHolder Not::Execute(Closure& closure, Context& context) {
		bool result = !runtime::IsTrue(argument_->Execute(closure, context));

		return ObjectHolder::Own(runtime::Bool(result));
	}

	Comparison::Comparison(Comparator cmp, unique_ptr<Statement> lhs, unique_ptr<Statement> rhs)
		: BinaryOperation(std::move(lhs), std::move(rhs)), cmp_(cmp) {		
	}

	ObjectHolder Comparison::Execute(Closure& closure, Context& context) {		
		bool result = cmp_(lhs_->Execute(closure, context), rhs_->Execute(closure, context), context);

		return ObjectHolder::Own(runtime::Bool(result));
	}

	NewInstance::NewInstance(const runtime::Class& class_, std::vector<std::unique_ptr<Statement>> args)
		: class_instance_{ class_ }, args_{ std::move(args) } {
	}

	NewInstance::NewInstance(const runtime::Class& class_)
		: NewInstance{ class_, {} } {
	}

	ObjectHolder NewInstance::Execute(Closure& closure, Context& context) {		
		if (class_instance_.HasMethod(INIT_METHOD, args_.size())) {
			std::vector<runtime::ObjectHolder> actual_args;
			for (auto& arg : args_) {
				actual_args.push_back(arg->Execute(closure, context));
			}
			class_instance_.Call(INIT_METHOD, actual_args, context);
		}
		return runtime::ObjectHolder::Share(class_instance_);
	}

	MethodBody::MethodBody(std::unique_ptr<Statement>&& body)
		: body_(std::move(body)) {
	}	

	ObjectHolder MethodBody::Execute(Closure& closure, Context& context) {
		try	{
			body_->Execute(closure, context);
		}
		catch (const ReturnException& exception_result) {
			return exception_result.GetStatement();
		}

		return None{}.Execute(closure, context);
	}

}  // namespace ast
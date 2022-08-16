#include "runtime.h"

#include <cassert>
#include <optional>
#include <sstream>

using namespace std;

namespace runtime {

	namespace {
		const string EQ_METHOD = "__eq__";
		const string LT_METHOD = "__lt__";
		const string STR_METHOD = "__str__"s;
	}  // namespace

	// -------------------------- ObjectHolder -----------------------------
	ObjectHolder::ObjectHolder(std::shared_ptr<Object> data)
		: data_(std::move(data)) {
	}

	void ObjectHolder::AssertIsValid() const {
		assert(data_ != nullptr);
	}

	ObjectHolder ObjectHolder::Share(Object& object) {
		// Возвращаем невладеющий shared_ptr (его deleter ничего не делает)
		return ObjectHolder(std::shared_ptr<Object>(&object, [](auto* /*p*/) { /* do nothing */ }));
	}

	ObjectHolder ObjectHolder::None() {
		return ObjectHolder();
	}

	Object& ObjectHolder::operator*() const {
		AssertIsValid();
		return *Get();
	}

	Object* ObjectHolder::operator->() const {
		AssertIsValid();
		return Get();
	}

	Object* ObjectHolder::Get() const {
		return data_.get();
	}

	ObjectHolder::operator bool() const {
		return Get() != nullptr;
	}
	// -------------------------------------------------------------------

	bool IsTrue(const ObjectHolder& object) {
		if (object.TryAs<Number>() != nullptr) {
			return object.TryAs<Number>()->GetValue();
		}
		else if (object.TryAs<String>() != nullptr) {
			if (!object.TryAs<String>()->GetValue().empty()) {
				return true;
			}
		}
		else if (object.TryAs<Bool>()) {
			return object.TryAs<Bool>()->GetValue();
		}
		return false;
	}

	void ClassInstance::Print(std::ostream& os, Context& context) {		
		if (HasMethod(STR_METHOD, 0)) {
			Call(STR_METHOD, {}, context)->Print(os, context);
		}
		else {
			os << this;
		}
	}

	bool ClassInstance::HasMethod(const std::string& method, size_t argument_count) const {
		const Method* method_ptr = cls_.GetMethod(method);
		if (method_ptr != nullptr && method_ptr->formal_params.size() == argument_count) {
			return true;
		}
		else {
			return false;
		}
	}

	Closure& ClassInstance::Fields() {
		return closure_;
	}

	const Closure& ClassInstance::Fields() const {
		return closure_;
	}

	ClassInstance::ClassInstance(const Class& cls)
		: cls_(cls) {	
	}

	ObjectHolder ClassInstance::Call(const std::string& method,
		const std::vector<ObjectHolder>& actual_args,
		Context& context) {

		if (HasMethod(method, actual_args.size())) {
			const Method* method_ptr = cls_.GetMethod(method);

			Closure temp_closure;
			temp_closure["self"] = ObjectHolder::Share(*this);
			// Добавляем аргументы
			for (size_t i = 0; i < actual_args.size(); ++i) {
				temp_closure[method_ptr->formal_params[i]] = actual_args[i];
			}

			return method_ptr->body->Execute(temp_closure, context);
		}
		else {
			throw std::runtime_error("There is no such method!"s);
		}
	}

	Class::Class(std::string name, std::vector<Method> methods, const Class* parent)
		: name_(name), methods_(std::move(methods)), parent_(parent) {
		if (parent != nullptr) {

		}
	}

	const Method* Class::GetMethod(const std::string& name) const {
		// Проверяем ввначале в текущем классе а потом в родительских классах
		auto child = this;
		while (true) {
			const auto result = std::find_if(
				child->methods_.begin(),
				child->methods_.end(),
				[this, &name](const Method& method) {
					if (method.name == name) {
						return true;
					}
					return false;
				}
			);
			if (result != child->methods_.end()) {
				return &*result;
			}
			else if (child->parent_ != nullptr) {
				child = child->parent_;
			}
			// Метод не найден ни в текущем не в родительских классах
			else {
				return nullptr;
			}
		}
		return nullptr;
	}

	[[nodiscard]] const std::string& Class::GetName() const {
		return name_;
	}

	void Class::Print(ostream& os, Context& /*context*/) {
		os << "Class " << name_;
	}

	void Bool::Print(std::ostream& os, [[maybe_unused]] Context& /*context*/) {
		os << (GetValue() ? "True"sv : "False"sv);
	}

	bool Equal(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
		// Оба объекта None
		if (!lhs && !rhs) {
			return true;
		}
		// Если lhs объект
		else if (lhs.TryAs<runtime::ClassInstance>() != nullptr) {
			auto obj_ptr = lhs.TryAs<runtime::ClassInstance>();
			if (obj_ptr->HasMethod(EQ_METHOD, 1)) {
				return IsTrue(obj_ptr->Call(EQ_METHOD, { rhs }, context));
			}
		}
		else if (lhs.TryAs<runtime::Number>() != nullptr && rhs.TryAs<runtime::Number>() != nullptr) {
			return lhs.TryAs<runtime::Number>()->GetValue() == rhs.TryAs<runtime::Number>()->GetValue();
		}
		else if (lhs.TryAs<runtime::String>() != nullptr && rhs.TryAs<runtime::String>() != nullptr) {
			return lhs.TryAs<runtime::String>()->GetValue() == rhs.TryAs<runtime::String>()->GetValue();
		}
		else if (lhs.TryAs<runtime::Bool>() != nullptr && rhs.TryAs<runtime::Bool>() != nullptr) {
			return lhs.TryAs<runtime::Bool>()->GetValue() == rhs.TryAs<runtime::Bool>()->GetValue();
		}
		throw std::runtime_error("Cannot compare objects for equality"s);
	}

	bool Less(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
		// Если lhs объект
		if (lhs.TryAs<runtime::ClassInstance>() != nullptr) {
			auto obj_ptr = lhs.TryAs<runtime::ClassInstance>();
			if (obj_ptr->HasMethod(LT_METHOD, 1)) {
				return IsTrue(obj_ptr->Call(LT_METHOD, { rhs }, context));
			}
		}
		else if (lhs.TryAs<runtime::Number>() != nullptr && rhs.TryAs<runtime::Number>() != nullptr) {
			return lhs.TryAs<runtime::Number>()->GetValue() < rhs.TryAs<runtime::Number>()->GetValue();
		}
		else if (lhs.TryAs<runtime::String>() != nullptr && rhs.TryAs<runtime::String>() != nullptr) {
			return lhs.TryAs<runtime::String>()->GetValue() < rhs.TryAs<runtime::String>()->GetValue();
		}
		else if (lhs.TryAs<runtime::Bool>() != nullptr && rhs.TryAs<runtime::Bool>() != nullptr) {
			return lhs.TryAs<runtime::Bool>()->GetValue() < rhs.TryAs<runtime::Bool>()->GetValue();
		}
		throw std::runtime_error("Cannot compare objects for less"s);
	}

	bool NotEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
		return !Equal(lhs, rhs, context);
	}

	bool Greater(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
		return !Less(lhs, rhs, context) && NotEqual(lhs, rhs, context);
	}

	bool LessOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
		return !Greater(lhs, rhs, context);
	}

	bool GreaterOrEqual(const ObjectHolder& lhs, const ObjectHolder& rhs, Context& context) {
		return !Less(lhs, rhs, context);
	}

}  // namespace runtime
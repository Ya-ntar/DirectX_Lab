#ifndef CPP_DELEGATES
#define CPP_DELEGATES

#include <vector>
#include <memory>
#include <tuple>
#include <cstring>
#include <cstdlib>
#include <utility>
#include <type_traits>

#include "Exports.h"

#ifndef DELEGATE_ASSERT
#include <assert.h>
#define DELEGATE_ASSERT(expression, ...) assert(expression)
#endif

#ifndef DELEGATE_STATIC_ASSERT
#if __cplusplus >= 201103L
#define DELEGATE_STATIC_ASSERT(expression, msg) static_assert(expression, msg)
#else
#define DELEGATE_STATIC_ASSERT(expression, msg)
#endif
#endif

#ifndef DELEGATE_INLINE_ALLOCATION_SIZE
#define DELEGATE_INLINE_ALLOCATION_SIZE 32
#endif

#define DECLARE_DELEGATE(name, ...) \
using name = Delegate<void, __VA_ARGS__>

#define DECLARE_DELEGATE_RET(name, retValue, ...) \
using name = Delegate<retValue, __VA_ARGS__>

#define DECLARE_MULTICAST_DELEGATE(name, ...) \
using name = MulticastDelegate<__VA_ARGS__>; \
using name ## Delegate = MulticastDelegate<__VA_ARGS__>::DelegateT

#define DECLARE_EVENT(name, ownerType, ...) \
class name : public MulticastDelegate<__VA_ARGS__> \
{ \
private: \
	friend class ownerType; \
	using MulticastDelegate::Broadcast; \
	using MulticastDelegate::RemoveAll; \
	using MulticastDelegate::Remove; \
};

#if __cplusplus >= 201703L
#define NO_DISCARD [[nodiscard]]
#else
#define NO_DISCARD		
#endif

namespace _DelegatesInteral
{
	template<bool IsConst, typename Object, typename RetVal, typename ...Args>
	struct MemberFunction;

	template<typename Object, typename RetVal, typename ...Args>
	struct MemberFunction<true, Object, RetVal, Args...>
	{
		using Type = RetVal(Object::*)(Args...) const;
	};

	template<typename Object, typename RetVal, typename ...Args>
	struct MemberFunction<false, Object, RetVal, Args...>
	{
		using Type = RetVal(Object::*)(Args...);
	};

	static void* (*Alloc)(size_t size) = [](size_t size) { return malloc(size); };
	static void(*Free)(void* pPtr) = [](void* pPtr) { free(pPtr); };
	template<typename T>
	void DelegateDeleteFunc(T* pPtr)
	{
		pPtr->~T();
		Free(pPtr);
	}
}

namespace Delegates
{
	using AllocateCallback = void* (*)(size_t size);
	using FreeCallback = void(*)(void* pPtr);
	inline void SetAllocationCallbacks(AllocateCallback allocateCallback, FreeCallback freeCallback)
	{
		_DelegatesInteral::Alloc = allocateCallback;
		_DelegatesInteral::Free = freeCallback;
	}
}

class IDelegateBase
{
public:
	IDelegateBase() = default;
	virtual ~IDelegateBase() noexcept = default;
	virtual const void* GetOwner() const { return nullptr; }
};

template<typename RetVal, typename... Args>
class IDelegate : public IDelegateBase
{
public:
	virtual RetVal Execute(Args&&... args) = 0;
};

template<typename RetVal, typename... Args2>
class StaticDelegate;

template<typename RetVal, typename... Args, typename... Args2>
class StaticDelegate<RetVal(Args...), Args2...> : public IDelegate<RetVal, Args...>
{
public:
	using DelegateFunction = RetVal(*)(Args..., Args2...);

	StaticDelegate(DelegateFunction function, Args2&&... args)
		: m_Function(function), m_Payload(std::forward<Args2>(args)...)
	{}
	virtual RetVal Execute(Args&&... args) override
	{
		return Execute_Internal(std::forward<Args>(args)..., std::index_sequence_for<Args2...>());
	}
private:
	template<std::size_t... Is>
	RetVal Execute_Internal(Args&&... args, std::index_sequence<Is...>)
	{
		return m_Function(std::forward<Args>(args)..., std::get<Is>(m_Payload)...);
	}

	DelegateFunction m_Function;
	std::tuple<Args2...> m_Payload;
};

template<bool IsConst, typename T, typename RetVal, typename... Args2>
class RawDelegate;

template<bool IsConst, typename T, typename RetVal, typename... Args, typename... Args2>
class RawDelegate<IsConst, T, RetVal(Args...), Args2...> : public IDelegate<RetVal, Args...>
{
public:
	using DelegateFunction = typename _DelegatesInteral::MemberFunction<IsConst, T, RetVal, Args..., Args2...>::Type;

	RawDelegate(T* pObject, DelegateFunction function, Args2&&... args)
		: m_pObject(pObject), m_Function(function), m_Payload(std::forward<Args2>(args)...)
	{}
	virtual RetVal Execute(Args&&... args) override
	{
		return Execute_Internal(std::forward<Args>(args)..., std::index_sequence_for<Args2...>());
	}
	virtual const void* GetOwner() const override
	{
		return m_pObject;
	}

private:
	template<std::size_t... Is>
	RetVal Execute_Internal(Args&&... args, std::index_sequence<Is...>)
	{
		return (m_pObject->*m_Function)(std::forward<Args>(args)..., std::get<Is>(m_Payload)...);
	}

	T* m_pObject;
	DelegateFunction m_Function;
	std::tuple<Args2...> m_Payload;
};

template<typename TLambda, typename RetVal, typename... Args>
class LambdaDelegate;

template<typename TLambda, typename RetVal, typename... Args, typename... Args2>
class LambdaDelegate<TLambda, RetVal(Args...), Args2...> : public IDelegate<RetVal, Args...>
{
public:
	explicit LambdaDelegate(TLambda&& lambda, Args2&&... args) :
		m_Lambda(std::forward<TLambda>(lambda)),
		m_Payload(std::forward<Args2>(args)...)
	{}

	RetVal Execute(Args&&... args) override
	{
		return Execute_Internal(std::forward<Args>(args)..., std::index_sequence_for<Args2...>());
	}
private:
	template<std::size_t... Is>
	RetVal Execute_Internal(Args&&... args, std::index_sequence<Is...>)
	{
		return (RetVal)((m_Lambda)(std::forward<Args>(args)..., std::get<Is>(m_Payload)...));
	}

	TLambda m_Lambda;
	std::tuple<Args2...> m_Payload;
};

template<bool IsConst, typename T, typename RetVal, typename... Args>
class SPDelegate;

template<bool IsConst, typename RetVal, typename T, typename... Args, typename... Args2>
class SPDelegate<IsConst, T, RetVal(Args...), Args2...> : public IDelegate<RetVal, Args...>
{
public:
	using DelegateFunction = typename _DelegatesInteral::MemberFunction<IsConst, T, RetVal, Args..., Args2...>::Type;

	SPDelegate(const std::shared_ptr<T>& pObject, DelegateFunction pFunction, Args2&&... args) :
		m_pObject(pObject),
		m_pFunction(pFunction),
		m_Payload(std::forward<Args2>(args)...)
	{
	}

	virtual RetVal Execute(Args&&... args) override
	{
		return Execute_Internal(std::forward<Args>(args)..., std::index_sequence_for<Args2...>());
	}

	virtual const void* GetOwner() const override
	{
		return m_pObject.expired() ? nullptr : m_pObject.lock().get();
	}

private:
	template<std::size_t... Is>
	RetVal Execute_Internal(Args&&... args, std::index_sequence<Is...>)
	{
		if (m_pObject.expired())
		{
			return RetVal();
		}
		else
		{
			std::shared_ptr<T> pPinned = m_pObject.lock();
			return (pPinned.get()->*m_pFunction)(std::forward<Args>(args)..., std::get<Is>(m_Payload)...);
		}
	}

	std::weak_ptr<T> m_pObject;
	DelegateFunction m_pFunction;
	std::tuple<Args2...> m_Payload;
};

class DelegateHandle
{
public:
	constexpr DelegateHandle() noexcept
		: m_Id(INVALID_ID)
	{
	}

	explicit DelegateHandle(bool /*generateId*/) noexcept
		: m_Id(GetNewID())
	{
	}

	~DelegateHandle() noexcept = default;
	DelegateHandle(const DelegateHandle& other) = default;
	DelegateHandle& operator=(const DelegateHandle& other) = default;

	DelegateHandle(DelegateHandle&& other) noexcept
		: m_Id(other.m_Id)
	{
		other.Reset();
	}

	DelegateHandle& operator=(DelegateHandle&& other) noexcept
	{
		m_Id = other.m_Id;
		other.Reset();
		return *this;
	}

	operator bool() const noexcept
	{
		return IsValid();
	}

	bool operator==(const DelegateHandle& other) const noexcept
	{
		return m_Id == other.m_Id;
	}

	bool operator<(const DelegateHandle& other) const noexcept
	{
		return m_Id < other.m_Id;
	}

	bool IsValid() const noexcept
	{
		return m_Id != INVALID_ID;
	}

	void Reset() noexcept
	{
		m_Id = INVALID_ID;
	}

	constexpr static const unsigned int INVALID_ID = (unsigned int)~0;
private:
	unsigned int m_Id;
	GAMEFRAMEWORK_API static unsigned int CURRENT_ID;

	static int GetNewID()
	{
		unsigned int output = DelegateHandle::CURRENT_ID++;
		if (DelegateHandle::CURRENT_ID == INVALID_ID)
		{
			DelegateHandle::CURRENT_ID = 0;
		}
		return output;
	}
};

template<size_t MaxStackSize>
class InlineAllocator
{
public:
	constexpr InlineAllocator() noexcept
		: m_Size(0)
	{
		DELEGATE_STATIC_ASSERT(MaxStackSize > sizeof(void*), "MaxStackSize is smaller or equal to the size of a pointer. This will make the use of an InlineAllocator pointless. Please increase the MaxStackSize.");
	}

	~InlineAllocator() noexcept
	{
		Free();
	}

	InlineAllocator(const InlineAllocator& other)
		: m_Size(0)
	{
		if (other.HasAllocation())
		{
			memcpy(Allocate(other.m_Size), other.GetAllocation(), other.m_Size);
		}
		m_Size = other.m_Size;
	}

	InlineAllocator& operator=(const InlineAllocator& other)
	{
		if (other.HasAllocation())
		{
			memcpy(Allocate(other.m_Size), other.GetAllocation(), other.m_Size);
		}
		m_Size = other.m_Size;
		return *this;
	}

	InlineAllocator(InlineAllocator&& other) noexcept
		: m_Size(other.m_Size)
	{
		other.m_Size = 0;
		if (m_Size > MaxStackSize)
		{
			std::swap(pPtr, other.pPtr);
		}
		else
		{
			memcpy(Buffer, other.Buffer, m_Size);
		}
	}

	InlineAllocator& operator=(InlineAllocator&& other) noexcept
	{
		Free();
		m_Size = other.m_Size;
		other.m_Size = 0;
		if (m_Size > MaxStackSize)
		{
			std::swap(pPtr, other.pPtr);
		}
		else
		{
			memcpy(Buffer, other.Buffer, m_Size);
		}
		return *this;
	}

	void* Allocate(const size_t size)
	{
		if (m_Size != size)
		{
			Free();
			m_Size = size;
			if (size > MaxStackSize)
			{
				pPtr = _DelegatesInteral::Alloc(size);
				return pPtr;
			}
		}
		return (void*)Buffer;
	}

	void Free()
	{
		if (m_Size > MaxStackSize)
		{
			_DelegatesInteral::Free(pPtr);
		}
		m_Size = 0;
	}

	void* GetAllocation() const
	{
		if (HasAllocation())
		{
			return HasHeapAllocation() ? pPtr : (void*)Buffer;
		}
		else
		{
			return nullptr;
		}
	}

	size_t GetSize() const
	{
		return m_Size;
	}

	bool HasAllocation() const
	{
		return m_Size > 0;
	}

	bool HasHeapAllocation() const
	{
		return m_Size > MaxStackSize;
	}

private:
	union
	{
		char Buffer[MaxStackSize];
		void* pPtr;
	};
	size_t m_Size;
};

class DelegateBase
{
public:
	constexpr DelegateBase() noexcept
		: m_Allocator()
	{
	}

	virtual ~DelegateBase() noexcept
	{
		Release();
	}

	DelegateBase(const DelegateBase& other)
		: m_Allocator(other.m_Allocator)
	{
	}

	DelegateBase& operator=(const DelegateBase& other)
	{
		Release();
		m_Allocator = other.m_Allocator;
		return *this;
	}

	DelegateBase(DelegateBase&& other) noexcept
		: m_Allocator(std::move(other.m_Allocator))
	{
	}

	DelegateBase& operator=(DelegateBase&& other) noexcept
	{
		Release();
		m_Allocator = std::move(other.m_Allocator);
		return *this;
	}

	const void* GetOwner() const
	{
		if (m_Allocator.HasAllocation())
		{
			return GetDelegate()->GetOwner();
		}
		return nullptr;
	}

	size_t GetSize() const
	{
		return m_Allocator.GetSize();
	}

	void ClearIfBoundTo(void* pObject)
	{
		if (pObject != nullptr && IsBoundTo(pObject))
		{
			Release();
		}
	}

	void Clear()
	{
		Release();
	}

	bool IsBound() const
	{
		return m_Allocator.HasAllocation();
	}

	bool IsBoundTo(void* pObject) const
	{
		if (pObject == nullptr || m_Allocator.HasAllocation() == false)
		{
			return false;
		}
		return GetDelegate()->GetOwner() == pObject;
	}

protected:
	void Release()
	{
		if (m_Allocator.HasAllocation())
		{
			GetDelegate()->~IDelegateBase();
			m_Allocator.Free();
		}
	}

	IDelegateBase* GetDelegate() const
	{
		return static_cast<IDelegateBase*>(m_Allocator.GetAllocation());
	}

	InlineAllocator<DELEGATE_INLINE_ALLOCATION_SIZE> m_Allocator;
};

template<typename RetVal, typename... Args>
class Delegate : public DelegateBase
{
private:
	template<typename T, typename... Args2>
	using ConstMemberFunction = typename _DelegatesInteral::MemberFunction<true, T, RetVal, Args..., Args2...>::Type;
	template<typename T, typename... Args2>
	using NonConstMemberFunction = typename _DelegatesInteral::MemberFunction<false, T, RetVal, Args..., Args2...>::Type;

public:
	using IDelegateT = IDelegate<RetVal, Args...>;

	template<typename T, typename... Args2>
	NO_DISCARD static Delegate CreateRaw(T* pObj, NonConstMemberFunction<T, Args2...> pFunction, Args2... args)
	{
		Delegate handler;
		handler.Bind<RawDelegate<false, T, RetVal(Args...), Args2...>>(pObj, pFunction, std::forward<Args2>(args)...);
		return handler;
	}

	template<typename T, typename... Args2>
	NO_DISCARD static Delegate CreateRaw(T* pObj, ConstMemberFunction<T, Args2...> pFunction, Args2... args)
	{
		Delegate handler;
		handler.Bind<RawDelegate<true, T, RetVal(Args...), Args2...>>(pObj, pFunction, std::forward<Args2>(args)...);
		return handler;
	}

	template<typename... Args2>
	NO_DISCARD static Delegate CreateStatic(RetVal(*pFunction)(Args..., Args2...), Args2... args)
	{
		Delegate handler;
		handler.Bind<StaticDelegate<RetVal(Args...), Args2...>>(pFunction, std::forward<Args2>(args)...);
		return handler;
	}

	template<typename T, typename... Args2>
	NO_DISCARD static Delegate CreateSP(const std::shared_ptr<T>& pObject, NonConstMemberFunction<T, Args2...> pFunction, Args2... args)
	{
		Delegate handler;
		handler.Bind<SPDelegate<false, T, RetVal(Args...), Args2...>>(pObject, pFunction, std::forward<Args2>(args)...);
		return handler;
	}

	template<typename T, typename... Args2>
	NO_DISCARD static Delegate CreateSP(const std::shared_ptr<T>& pObject, ConstMemberFunction<T, Args2...> pFunction, Args2... args)
	{
		Delegate handler;
		handler.Bind<SPDelegate<true, T, RetVal(Args...), Args2...>>(pObject, pFunction, std::forward<Args2>(args)...);
		return handler;
	}

	template<typename TLambda, typename... Args2>
	NO_DISCARD static Delegate CreateLambda(TLambda&& lambda, Args2... args)
	{
		Delegate handler;
		handler.Bind<LambdaDelegate<TLambda, RetVal(Args...), Args2...>>(std::forward<TLambda>(lambda), std::forward<Args2>(args)...);
		return handler;
	}

	template<typename T, typename... Args2>
	void BindRaw(T* pObject, NonConstMemberFunction<T, Args2...> pFunction, Args2&&... args)
	{
		DELEGATE_STATIC_ASSERT(!std::is_const<T>::value, "Cannot bind a non-const function on a const object");
		*this = CreateRaw<T, Args2... >(pObject, pFunction, std::forward<Args2>(args)...);
	}

	template<typename T, typename... Args2>
	void BindRaw(T* pObject, ConstMemberFunction<T, Args2...> pFunction, Args2&&... args)
	{
		*this = CreateRaw<T, Args2... >(pObject, pFunction, std::forward<Args2>(args)...);
	}

	template<typename... Args2>
	void BindStatic(RetVal(*pFunction)(Args..., Args2...), Args2&&... args)
	{
		*this = CreateStatic<Args2... >(pFunction, std::forward<Args2>(args)...);
	}

	template<typename LambdaType, typename... Args2>
	void BindLambda(LambdaType&& lambda, Args2&&... args)
	{
		*this = CreateLambda<LambdaType, Args2... >(std::forward<LambdaType>(lambda), std::forward<Args2>(args)...);
	}

	template<typename T, typename... Args2>
	void BindSP(std::shared_ptr<T> pObject, NonConstMemberFunction<T, Args2...> pFunction, Args2&&... args)
	{
		DELEGATE_STATIC_ASSERT(!std::is_const<T>::value, "Cannot bind a non-const function on a const object");
		*this = CreateSP<T, Args2... >(pObject, pFunction, std::forward<Args2>(args)...);
	}

	template<typename T, typename... Args2>
	void BindSP(std::shared_ptr<T> pObject, ConstMemberFunction<T, Args2...> pFunction, Args2&&... args)
	{
		*this = CreateSP<T, Args2... >(pObject, pFunction, std::forward<Args2>(args)...);
	}

	RetVal Execute(Args... args) const
	{
		DELEGATE_ASSERT(m_Allocator.HasAllocation(), "Delegate is not bound");
		return ((IDelegateT*)GetDelegate())->Execute(std::forward<Args>(args)...);
	}

	RetVal ExecuteIfBound(Args... args) const
	{
		if (IsBound())
		{
			return ((IDelegateT*)GetDelegate())->Execute(std::forward<Args>(args)...);
		}
		return RetVal();
	}

private:
	template<typename T, typename... Args3>
	void Bind(Args3&&... args)
	{
		Release();
		void* pAlloc = m_Allocator.Allocate(sizeof(T));
		new (pAlloc) T(std::forward<Args3>(args)...);
	}
};


class MulticastDelegateBase
{
public:
	virtual ~MulticastDelegateBase() = default;
};

template<typename... Args>
class MulticastDelegate : public MulticastDelegateBase
{
public:
	using DelegateT = Delegate<void, Args...>;

private:
	struct DelegateHandlerPair
	{
		DelegateHandle Handle;
		DelegateT Callback;
		DelegateHandlerPair() : Handle(false) {}
		DelegateHandlerPair(const DelegateHandle& handle, const DelegateT& callback) : Handle(handle), Callback(callback) {}
		DelegateHandlerPair(const DelegateHandle& handle, DelegateT&& callback) : Handle(handle), Callback(std::move(callback)) {}
	};
	template<typename T, typename... Args2>
	using ConstMemberFunction = typename _DelegatesInteral::MemberFunction<true, T, void, Args..., Args2...>::Type;
	template<typename T, typename... Args2>
	using NonConstMemberFunction = typename _DelegatesInteral::MemberFunction<false, T, void, Args..., Args2...>::Type;

public:
	constexpr MulticastDelegate()
		: m_Locks(0)
	{
	}

	~MulticastDelegate() noexcept = default;

	MulticastDelegate(const MulticastDelegate& other) = default;

	MulticastDelegate& operator=(const MulticastDelegate& other) = default;

	MulticastDelegate(MulticastDelegate&& other) noexcept
		: m_Events(std::move(other.m_Events)),
		m_Locks(std::move(other.m_Locks))
	{
	}

	MulticastDelegate& operator=(MulticastDelegate&& other) noexcept
	{
		m_Events = std::move(other.m_Events);
		m_Locks = std::move(other.m_Locks);
		return *this;
	}

	DelegateHandle operator+=(DelegateT&& handler) noexcept
	{
		return Add(std::forward<DelegateT>(handler));
	}

	bool operator-=(DelegateHandle& handle)
	{
		return Remove(handle);
	}

	DelegateHandle Add(DelegateT&& handler) noexcept
	{
		for (size_t i = 0; i < m_Events.size(); ++i)
		{
			if (m_Events[i].Handle.IsValid() == false)
			{
				m_Events[i] = DelegateHandlerPair(DelegateHandle(true), std::move(handler));
				return m_Events[i].Handle;
			}
		}
		m_Events.emplace_back(DelegateHandle(true), std::move(handler));
		return m_Events.back().Handle;
	}

	template<typename T, typename... Args2>
	DelegateHandle AddRaw(T* pObject, NonConstMemberFunction<T, Args2...> pFunction, Args2&&... args)
	{
		return Add(DelegateT::CreateRaw(pObject, pFunction, std::forward<Args2>(args)...));
	}

	template<typename T, typename... Args2>
	DelegateHandle AddRaw(T* pObject, ConstMemberFunction<T, Args2...> pFunction, Args2&&... args)
	{
		return Add(DelegateT::CreateRaw(pObject, pFunction, std::forward<Args2>(args)...));
	}

	template<typename... Args2>
	DelegateHandle AddStatic(void(*pFunction)(Args..., Args2...), Args2&&... args)
	{
		return Add(DelegateT::CreateStatic(pFunction, std::forward<Args2>(args)...));
	}

	template<typename LambdaType, typename... Args2>
	DelegateHandle AddLambda(LambdaType&& lambda, Args2&&... args)
	{
		return Add(DelegateT::CreateLambda(std::forward<LambdaType>(lambda), std::forward<Args2>(args)...));
	}

	template<typename T, typename... Args2>
	DelegateHandle AddSP(std::shared_ptr<T> pObject, NonConstMemberFunction<T, Args2...> pFunction, Args2&&... args)
	{
		return Add(DelegateT::CreateSP(pObject, pFunction, std::forward<Args2>(args)...));
	}

	template<typename T, typename... Args2>
	DelegateHandle AddSP(std::shared_ptr<T> pObject, ConstMemberFunction<T, Args2...> pFunction, Args2&&... args)
	{
		return Add(DelegateT::CreateSP(pObject, pFunction, std::forward<Args2>(args)...));
	}

	void RemoveObject(void* pObject)
	{
		if (pObject != nullptr)
		{
			for (size_t i = 0; i < m_Events.size(); ++i)
			{
				if (m_Events[i].Callback.GetOwner() == pObject)
				{
					if (IsLocked())
					{
						m_Events[i].Clear();
					}
					else
					{
						std::swap(m_Events[i], m_Events[m_Events.size() - 1]);
						m_Events.pop_back();
					}
				}
			}
		}
	}

	bool Remove(DelegateHandle& handle)
	{
		if (handle.IsValid())
		{
			for (size_t i = 0; i < m_Events.size(); ++i)
			{
				if (m_Events[i].Handle == handle)
				{
					if (IsLocked())
					{
						m_Events[i].Callback.Clear();
					}
					else
					{
						std::swap(m_Events[i], m_Events[m_Events.size() - 1]);
						m_Events.pop_back();
					}
					handle.Reset();
					return true;
				}
			}
		}
		return false;
	}

	bool IsBoundTo(const DelegateHandle& handle) const
	{
		if (handle.IsValid())
		{
			for (size_t i = 0; i < m_Events.size(); ++i)
			{
				if (m_Events[i].Handle == handle)
				{
					return true;
				}
			}
		}
		return false;
	}

	void RemoveAll()
	{
		if (IsLocked())
		{
			for (DelegateHandlerPair& handler : m_Events)
			{
				handler.Callback.Clear();
			}
		}
		else
		{
			m_Events.clear();
		}
	}

	void Compress(const size_t maxSpace = 0)
	{
		if (IsLocked() == false)
		{
			size_t toDelete = 0;
			for (size_t i = 0; i < m_Events.size() - toDelete; ++i)
			{
				if (m_Events[i].Handle.IsValid() == false)
				{
					std::swap(m_Events[i], m_Events[toDelete]);
					++toDelete;
				}
			}
			if (toDelete > maxSpace)
			{
				m_Events.resize(m_Events.size() - toDelete);
			}
		}
	}

	void Broadcast(Args ...args)
	{
		Lock();
		for (size_t i = 0; i < m_Events.size(); ++i)
		{
			if (m_Events[i].Handle.IsValid())
			{
				m_Events[i].Callback.Execute(std::forward<Args>(args)...);
			}
		}
		Unlock();
	}

	size_t GetSize() const
	{
		return m_Events.size();
	}

private:
	void Lock()
	{
		++m_Locks;
	}

	void Unlock()
	{
		DELEGATE_ASSERT(m_Locks > 0);
		--m_Locks;
	}

	bool IsLocked() const
	{
		return m_Locks > 0;
	}

	std::vector<DelegateHandlerPair> m_Events;
	unsigned int m_Locks;
};

#endif

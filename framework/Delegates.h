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

namespace _DelegatesInteral {
    template<bool IsConst, typename Object, typename RetVal, typename ...Args>
    struct MemberFunction;

    template<typename Object, typename RetVal, typename ...Args>
    struct MemberFunction<true, Object, RetVal, Args...> {
        using Type = RetVal(Object::*)(Args...) const;
    };

    template<typename Object, typename RetVal, typename ...Args>
    struct MemberFunction<false, Object, RetVal, Args...> {
        using Type = RetVal(Object::*)(Args...);
    };

    static void *(*Alloc)(size_t size) = [](size_t size) { return malloc(size); };

    static void (*Free)(void *pPtr) = [](void *pPtr) { free(pPtr); };

    template<typename T>
    void DelegateDeleteFunc(T *pPtr) {
        pPtr->~T();
        Free(pPtr);
    }
}

namespace Delegates {
    using AllocateCallback = void *(*)(size_t size);
    using FreeCallback = void (*)(void *ptr);

    inline void SetAllocationCallbacks(AllocateCallback allocateCallback, FreeCallback freeCallback) {
        _DelegatesInteral::Alloc = allocateCallback;
        _DelegatesInteral::Free = freeCallback;
    }
}

class IDelegateBase {
public:
    IDelegateBase() = default;

    virtual ~IDelegateBase() noexcept = default;

    virtual const void *GetOwner() const { return nullptr; }
};

template<typename RetVal, typename... Args>
class IDelegate : public IDelegateBase {
public:
    virtual RetVal Execute(Args &&... args) = 0;
};

template<typename RetVal, typename... Args2>
class StaticDelegate;

template<typename RetVal, typename... Args, typename... Args2>
class StaticDelegate<RetVal(Args...), Args2...> : public IDelegate<RetVal, Args...> {
public:
    using DelegateFunction = RetVal(*)(Args..., Args2...);

    StaticDelegate(DelegateFunction function, Args2 &&... args)
            : function_(function), payload_(std::forward<Args2>(args)...) {}

    virtual RetVal Execute(Args &&... args) override {
        return Execute_Internal(std::forward<Args>(args)..., std::index_sequence_for<Args2...>());
    }

private:
    template<std::size_t... Is>
    RetVal Execute_Internal(Args &&... args, std::index_sequence<Is...>) {
        return function_(std::forward<Args>(args)..., std::get<Is>(payload_)...);
    }

    DelegateFunction function_;
    std::tuple<Args2...> payload_;
};

template<bool IsConst, typename T, typename RetVal, typename... Args2>
class RawDelegate;

template<bool IsConst, typename T, typename RetVal, typename... Args, typename... Args2>
class RawDelegate<IsConst, T, RetVal(Args...), Args2...> : public IDelegate<RetVal, Args...> {
public:
    using DelegateFunction = typename _DelegatesInteral::MemberFunction<IsConst, T, RetVal, Args..., Args2...>::Type;

    RawDelegate(T *object, DelegateFunction function, Args2 &&... args)
            : object_(object), function_(function), payload_(std::forward<Args2>(args)...) {}

    virtual RetVal Execute(Args &&... args) override {
        return Execute_Internal(std::forward<Args>(args)..., std::index_sequence_for<Args2...>());
    }

    virtual const void *GetOwner() const override {
        return object_;
    }

private:
    template<std::size_t... Is>
    RetVal Execute_Internal(Args &&... args, std::index_sequence<Is...>) {
        return (object_->*function_)(std::forward<Args>(args)..., std::get<Is>(payload_)...);
    }

    T *object_;
    DelegateFunction function_;
    std::tuple<Args2...> payload_;
};

template<typename TLambda, typename RetVal, typename... Args>
class LambdaDelegate;

template<typename TLambda, typename RetVal, typename... Args, typename... Args2>
class LambdaDelegate<TLambda, RetVal(Args...), Args2...> : public IDelegate<RetVal, Args...> {
public:
    explicit LambdaDelegate(TLambda &&lambda, Args2 &&... args) :
            lambda_(std::forward<TLambda>(lambda)),
            payload_(std::forward<Args2>(args)...) {}

    RetVal Execute(Args &&... args) override {
        return Execute_Internal(std::forward<Args>(args)..., std::index_sequence_for<Args2...>());
    }

private:
    template<std::size_t... Is>
    RetVal Execute_Internal(Args &&... args, std::index_sequence<Is...>) {
        return (RetVal) ((lambda_)(std::forward<Args>(args)..., std::get<Is>(payload_)...));
    }

    TLambda lambda_;
    std::tuple<Args2...> payload_;
};

template<bool IsConst, typename T, typename RetVal, typename... Args>
class SPDelegate;

template<bool IsConst, typename RetVal, typename T, typename... Args, typename... Args2>
class SPDelegate<IsConst, T, RetVal(Args...), Args2...> : public IDelegate<RetVal, Args...> {
public:
    using DelegateFunction = typename _DelegatesInteral::MemberFunction<IsConst, T, RetVal, Args..., Args2...>::Type;

    SPDelegate(const std::shared_ptr<T> &object, DelegateFunction function, Args2 &&... args) :
            object_(object),
            function_(function),
            payload_(std::forward<Args2>(args)...) {
    }

    virtual RetVal Execute(Args &&... args) override {
        return Execute_Internal(std::forward<Args>(args)..., std::index_sequence_for<Args2...>());
    }

    virtual const void *GetOwner() const override {
        return object_.expired() ? nullptr : object_.lock().get();
    }

private:
    template<std::size_t... Is>
    RetVal Execute_Internal(Args &&... args, std::index_sequence<Is...>) {
        if (object_.expired()) {
            return RetVal();
        } else {
            std::shared_ptr<T> pinned = object_.lock();
            return (pinned.get()->*function_)(std::forward<Args>(args)..., std::get<Is>(payload_)...);
        }
    }

    std::weak_ptr<T> object_;
    DelegateFunction function_;
    std::tuple<Args2...> payload_;
};

class DelegateHandle {
public:
    constexpr DelegateHandle() noexcept
            : id_(INVALID_ID) {
    }

    explicit DelegateHandle(bool /*generateId*/) noexcept
            : id_(GetNewID()) {
    }

    ~DelegateHandle() noexcept = default;

    DelegateHandle(const DelegateHandle &other) = default;

    DelegateHandle &operator=(const DelegateHandle &other) = default;

    DelegateHandle(DelegateHandle &&other) noexcept
            : id_(other.id_) {
        other.Reset();
    }

    DelegateHandle &operator=(DelegateHandle &&other) noexcept {
        id_ = other.id_;
        other.Reset();
        return *this;
    }

    operator bool() const noexcept {
        return IsValid();
    }

    bool operator==(const DelegateHandle &other) const noexcept {
        return id_ == other.id_;
    }

    bool operator<(const DelegateHandle &other) const noexcept {
        return id_ < other.id_;
    }

    bool IsValid() const noexcept {
        return id_ != INVALID_ID;
    }

    void Reset() noexcept {
        id_ = INVALID_ID;
    }

    constexpr static const unsigned int INVALID_ID = (unsigned int) ~0;
private:
    unsigned int id_;
    GAMEFRAMEWORK_API static unsigned int CURRENT_ID;

    static int GetNewID() {
        unsigned int output = DelegateHandle::CURRENT_ID++;
        if (DelegateHandle::CURRENT_ID == INVALID_ID) {
            DelegateHandle::CURRENT_ID = 0;
        }
        return output;
    }
};

template<size_t MaxStackSize>
class InlineAllocator {
public:
    constexpr InlineAllocator() noexcept
            : size_(0) {
        DELEGATE_STATIC_ASSERT(MaxStackSize > sizeof(void *),
                               "MaxStackSize is smaller or equal to the size of a pointer. This will make the use of an InlineAllocator pointless. Please increase the MaxStackSize.");
    }

    ~InlineAllocator() noexcept {
        Free();
    }

    InlineAllocator(const InlineAllocator &other)
            : size_(0) {
        if (other.HasAllocation()) {
            memcpy(Allocate(other.size_), other.GetAllocation(), other.size_);
        }
        size_ = other.size_;
    }

    InlineAllocator &operator=(const InlineAllocator &other) {
        if (other.HasAllocation()) {
            memcpy(Allocate(other.size_), other.GetAllocation(), other.size_);
        }
        size_ = other.size_;
        return *this;
    }

    InlineAllocator(InlineAllocator &&other) noexcept
            : size_(other.size_) {
        other.size_ = 0;
        if (size_ > MaxStackSize) {
            std::swap(ptr_, other.ptr_);
        } else {
            memcpy(buffer_, other.buffer_, size_);
        }
    }

    InlineAllocator &operator=(InlineAllocator &&other) noexcept {
        Free();
        size_ = other.size_;
        other.size_ = 0;
        if (size_ > MaxStackSize) {
            std::swap(ptr_, other.ptr_);
        } else {
            memcpy(buffer_, other.buffer_, size_);
        }
        return *this;
    }

    void *Allocate(const size_t size) {
        if (size_ != size) {
            Free();
            size_ = size;
            if (size > MaxStackSize) {
                ptr_ = _DelegatesInteral::Alloc(size);
                return ptr_;
            }
        }
        return (void *) buffer_;
    }

    void Free() {
        if (size_ > MaxStackSize) {
            _DelegatesInteral::Free(ptr_);
        }
        size_ = 0;
    }

    void *GetAllocation() const {
        if (HasAllocation()) {
            return HasHeapAllocation() ? ptr_ : (void *) buffer_;
        } else {
            return nullptr;
        }
    }

    size_t GetSize() const {
        return size_;
    }

    bool HasAllocation() const {
        return size_ > 0;
    }

    bool HasHeapAllocation() const {
        return size_ > MaxStackSize;
    }

private:
    union {
        char buffer_[MaxStackSize];
        void *ptr_;
    };
    size_t size_;
};

class DelegateBase {
public:
    constexpr DelegateBase() noexcept = default;

    virtual ~DelegateBase() noexcept {
        Release();
    }

    DelegateBase(const DelegateBase &other)
            : allocator_(other.allocator_) {
    }

    DelegateBase &operator=(const DelegateBase &other) {
        Release();
        allocator_ = other.allocator_;
        return *this;
    }

    DelegateBase(DelegateBase &&other) noexcept
            : allocator_(std::move(other.allocator_)) {
    }

    DelegateBase &operator=(DelegateBase &&other) noexcept {
        Release();
        allocator_ = std::move(other.allocator_);
        return *this;
    }

    const void *GetOwner() const {
        if (allocator_.HasAllocation()) {
            return GetDelegate()->GetOwner();
        }
        return nullptr;
    }

    size_t GetSize() const {
        return allocator_.GetSize();
    }

    void ClearIfBoundTo(void *object) {
        if (object != nullptr && IsBoundTo(object)) {
            Release();
        }
    }

    void Clear() {
        Release();
    }

    bool IsBound() const {
        return allocator_.HasAllocation();
    }

    bool IsBoundTo(void *object) const {
        if (object == nullptr || allocator_.HasAllocation() == false) {
            return false;
        }
        return GetDelegate()->GetOwner() == object;
    }

protected:
    void Release() {
        if (allocator_.HasAllocation()) {
            GetDelegate()->~IDelegateBase();
            allocator_.Free();
        }
    }

    IDelegateBase *GetDelegate() const {
        return static_cast<IDelegateBase *>(allocator_.GetAllocation());
    }

    InlineAllocator<DELEGATE_INLINE_ALLOCATION_SIZE> allocator_;
};

template<typename RetVal, typename... Args>
class Delegate : public DelegateBase {
private:
    template<typename T, typename... Args2>
    using ConstMemberFunction = typename _DelegatesInteral::MemberFunction<true, T, RetVal, Args..., Args2...>::Type;
    template<typename T, typename... Args2>
    using NonConstMemberFunction = typename _DelegatesInteral::MemberFunction<false, T, RetVal, Args..., Args2...>::Type;

public:
    using IDelegateT = IDelegate<RetVal, Args...>;

    template<typename T, typename... Args2>
    NO_DISCARD static Delegate CreateRaw(T *object, NonConstMemberFunction<T, Args2...> function, Args2... args) {
        Delegate handler;
        handler.Bind<RawDelegate<false, T, RetVal(Args...), Args2...>>(object, function, std::forward<Args2>(args)...);
        return handler;
    }

    template<typename T, typename... Args2>
    NO_DISCARD static Delegate CreateRaw(T *object, ConstMemberFunction<T, Args2...> function, Args2... args) {
        Delegate handler;
        handler.Bind<RawDelegate<true, T, RetVal(Args...), Args2...>>(object, function, std::forward<Args2>(args)...);
        return handler;
    }

    template<typename... Args2>
    NO_DISCARD static Delegate CreateStatic(RetVal(*function)(Args..., Args2...), Args2... args) {
        Delegate handler;
        handler.Bind<StaticDelegate<RetVal(Args...), Args2...>>(function, std::forward<Args2>(args)...);
        return handler;
    }

    template<typename T, typename... Args2>
    NO_DISCARD static Delegate
    CreateSP(const std::shared_ptr<T> &object, NonConstMemberFunction<T, Args2...> function, Args2... args) {
        Delegate handler;
        handler.Bind<SPDelegate<false, T, RetVal(Args...), Args2...>>(object, function, std::forward<Args2>(args)...);
        return handler;
    }

    template<typename T, typename... Args2>
    NO_DISCARD static Delegate
    CreateSP(const std::shared_ptr<T> &object, ConstMemberFunction<T, Args2...> function, Args2... args) {
        Delegate handler;
        handler.Bind<SPDelegate<true, T, RetVal(Args...), Args2...>>(object, function, std::forward<Args2>(args)...);
        return handler;
    }

    template<typename TLambda, typename... Args2>
    NO_DISCARD static Delegate CreateLambda(TLambda &&lambda, Args2... args) {
        Delegate handler;
        handler.Bind<LambdaDelegate<TLambda, RetVal(Args...), Args2...>>(std::forward<TLambda>(lambda),
                                                                         std::forward<Args2>(args)...);
        return handler;
    }

    template<typename T, typename... Args2>
    void BindRaw(T *object, NonConstMemberFunction<T, Args2...> function, Args2 &&... args) {
        DELEGATE_STATIC_ASSERT(!std::is_const<T>::value, "Cannot bind a non-const function on a const object");
        *this = CreateRaw<T, Args2...>(object, function, std::forward<Args2>(args)...);
    }

    template<typename T, typename... Args2>
    void BindRaw(T *object, ConstMemberFunction<T, Args2...> function, Args2 &&... args) {
        *this = CreateRaw<T, Args2...>(object, function, std::forward<Args2>(args)...);
    }

    template<typename... Args2>
    void BindStatic(RetVal(*function)(Args..., Args2...), Args2 &&... args) {
        *this = CreateStatic<Args2...>(function, std::forward<Args2>(args)...);
    }

    template<typename LambdaType, typename... Args2>
    void BindLambda(LambdaType &&lambda, Args2 &&... args) {
        *this = CreateLambda<LambdaType, Args2...>(std::forward<LambdaType>(lambda), std::forward<Args2>(args)...);
    }

    template<typename T, typename... Args2>
    void BindSP(std::shared_ptr<T> object, NonConstMemberFunction<T, Args2...> function, Args2 &&... args) {
        DELEGATE_STATIC_ASSERT(!std::is_const<T>::value, "Cannot bind a non-const function on a const object");
        *this = CreateSP<T, Args2...>(object, function, std::forward<Args2>(args)...);
    }

    template<typename T, typename... Args2>
    void BindSP(std::shared_ptr<T> object, ConstMemberFunction<T, Args2...> function, Args2 &&... args) {
        *this = CreateSP<T, Args2...>(object, function, std::forward<Args2>(args)...);
    }

    RetVal Execute(Args... args) const {
        DELEGATE_ASSERT(allocator_.HasAllocation(), "Delegate is not bound");
        return ((IDelegateT *) GetDelegate())->Execute(std::forward<Args>(args)...);
    }

    RetVal ExecuteIfBound(Args... args) const {
        if (IsBound()) {
            return ((IDelegateT *) GetDelegate())->Execute(std::forward<Args>(args)...);
        }
        return RetVal();
    }

private:
    template<typename T, typename... Args3>
    void Bind(Args3 &&... args) {
        Release();
        void *alloc = allocator_.Allocate(sizeof(T));
        new(alloc) T(std::forward<Args3>(args)...);
    }
};


class MulticastDelegateBase {
public:
    virtual ~MulticastDelegateBase() = default;
};

template<typename... Args>
class MulticastDelegate : public MulticastDelegateBase {
public:
    using DelegateT = Delegate<void, Args...>;

private:
    struct DelegateHandlerPair {
        DelegateHandle handle;
        DelegateT callback;

        DelegateHandlerPair() : handle(false) {}

        DelegateHandlerPair(const DelegateHandle &h, const DelegateT &cb) : handle(h), callback(cb) {}

        DelegateHandlerPair(const DelegateHandle &h, DelegateT &&cb) : handle(h), callback(std::move(cb)) {}
    };

    template<typename T, typename... Args2>
    using ConstMemberFunction = typename _DelegatesInteral::MemberFunction<true, T, void, Args..., Args2...>::Type;
    template<typename T, typename... Args2>
    using NonConstMemberFunction = typename _DelegatesInteral::MemberFunction<false, T, void, Args..., Args2...>::Type;

public:
    constexpr MulticastDelegate()
            : locks_(0) {
    }

    ~MulticastDelegate() noexcept = default;

    MulticastDelegate(const MulticastDelegate &other) = default;

    MulticastDelegate &operator=(const MulticastDelegate &other) = default;

    MulticastDelegate(MulticastDelegate &&other) noexcept
            : events_(std::move(other.events_)),
              locks_(std::move(other.locks_)) {
    }

    MulticastDelegate &operator=(MulticastDelegate &&other) noexcept {
        events_ = std::move(other.events_);
        locks_ = std::move(other.locks_);
        return *this;
    }

    DelegateHandle operator+=(DelegateT &&handler) noexcept {
        return Add(std::forward<DelegateT>(handler));
    }

    bool operator-=(DelegateHandle &handle) {
        return Remove(handle);
    }

    DelegateHandle Add(DelegateT &&handler) noexcept {
        for (size_t i = 0; i < events_.size(); ++i) {
            if (events_[i].handle.IsValid() == false) {
                events_[i] = DelegateHandlerPair(DelegateHandle(true), std::move(handler));
                return events_[i].handle;
            }
        }
        events_.emplace_back(DelegateHandle(true), std::move(handler));
        return events_.back().handle;
    }

    template<typename T, typename... Args2>
    DelegateHandle AddRaw(T *object, NonConstMemberFunction<T, Args2...> function, Args2 &&... args) {
        return Add(DelegateT::CreateRaw(object, function, std::forward<Args2>(args)...));
    }

    template<typename T, typename... Args2>
    DelegateHandle AddRaw(T *object, ConstMemberFunction<T, Args2...> function, Args2 &&... args) {
        return Add(DelegateT::CreateRaw(object, function, std::forward<Args2>(args)...));
    }

    template<typename... Args2>
    DelegateHandle AddStatic(void(*function)(Args..., Args2...), Args2 &&... args) {
        return Add(DelegateT::CreateStatic(function, std::forward<Args2>(args)...));
    }

    template<typename LambdaType, typename... Args2>
    DelegateHandle AddLambda(LambdaType &&lambda, Args2 &&... args) {
        return Add(DelegateT::CreateLambda(std::forward<LambdaType>(lambda), std::forward<Args2>(args)...));
    }

    template<typename T, typename... Args2>
    DelegateHandle AddSP(std::shared_ptr<T> object, NonConstMemberFunction<T, Args2...> function, Args2 &&... args) {
        return Add(DelegateT::CreateSP(object, function, std::forward<Args2>(args)...));
    }

    template<typename T, typename... Args2>
    DelegateHandle AddSP(std::shared_ptr<T> object, ConstMemberFunction<T, Args2...> function, Args2 &&... args) {
        return Add(DelegateT::CreateSP(object, function, std::forward<Args2>(args)...));
    }

    void RemoveObject(void *object) {
        if (object != nullptr) {
            for (size_t i = 0; i < events_.size(); ++i) {
                if (events_[i].callback.GetOwner() == object) {
                    if (IsLocked()) {
                        events_[i].callback.Clear();
                    } else {
                        std::swap(events_[i], events_[events_.size() - 1]);
                        events_.pop_back();
                    }
                }
            }
        }
    }

    bool Remove(DelegateHandle &handle) {
        if (handle.IsValid()) {
            for (size_t i = 0; i < events_.size(); ++i) {
                if (events_[i].handle == handle) {
                    if (IsLocked()) {
                        events_[i].callback.Clear();
                    } else {
                        std::swap(events_[i], events_[events_.size() - 1]);
                        events_.pop_back();
                    }
                    handle.Reset();
                    return true;
                }
            }
        }
        return false;
    }

    bool IsBoundTo(const DelegateHandle &handle) const {
        if (handle.IsValid()) {
            for (size_t i = 0; i < events_.size(); ++i) {
                if (events_[i].handle == handle) {
                    return true;
                }
            }
        }
        return false;
    }

    void RemoveAll() {
        if (IsLocked()) {
            for (DelegateHandlerPair &handler: events_) {
                handler.callback.Clear();
            }
        } else {
            events_.clear();
        }
    }

    void Compress(const size_t max_space = 0) {
        if (IsLocked() == false) {
            size_t to_delete = 0;
            for (size_t i = 0; i < events_.size() - to_delete; ++i) {
                if (events_[i].handle.IsValid() == false) {
                    std::swap(events_[i], events_[to_delete]);
                    ++to_delete;
                }
            }
            if (to_delete > max_space) {
                events_.resize(events_.size() - to_delete);
            }
        }
    }

    void Broadcast(Args ...args) {
        Lock();
        for (size_t i = 0; i < events_.size(); ++i) {
            if (events_[i].handle.IsValid()) {
                events_[i].callback.Execute(std::forward<Args>(args)...);
            }
        }
        Unlock();
    }

    size_t GetSize() const {
        return events_.size();
    }

private:
    void Lock() {
        ++locks_;
    }

    void Unlock() {
        DELEGATE_ASSERT(locks_ > 0);
        --locks_;
    }

    bool IsLocked() const {
        return locks_ > 0;
    }

    std::vector<DelegateHandlerPair> events_;
    unsigned int locks_;
};

#endif

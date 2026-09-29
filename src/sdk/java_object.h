#pragma once
#include "pch.h"
#include "core/jvm_wrapper.h"
#include "config/mappings.h"

/*
 * JavaObject — RAII wrapper for a JNI jobject (global ref).
 * Prevents leaks and provides safe move semantics.
 */
class JavaObject {
public:
    JavaObject() : m_obj(nullptr) {}

    explicit JavaObject(jobject localRef) : m_obj(nullptr) {
        if (localRef) {
            JNIEnv* env = JvmWrapper::getEnv();
            if (env) {
                m_obj = env->NewGlobalRef(localRef);
                env->DeleteLocalRef(localRef);
            }
        }
    }

    ~JavaObject() { release(); }

    // Move
    JavaObject(JavaObject&& other) noexcept : m_obj(other.m_obj) { other.m_obj = nullptr; }
    JavaObject& operator=(JavaObject&& other) noexcept {
        if (this != &other) {
            release();
            m_obj = other.m_obj;
            other.m_obj = nullptr;
        }
        return *this;
    }

    // No copy
    JavaObject(const JavaObject&) = delete;
    JavaObject& operator=(const JavaObject&) = delete;

    void release() {
        if (m_obj) {
            JNIEnv* env = JvmWrapper::getEnv();
            if (env) env->DeleteGlobalRef(m_obj);
            m_obj = nullptr;
        }
    }

    jobject get() const { return m_obj; }
    explicit operator bool() const { return m_obj != nullptr; }

    // Wrap a local ref into this object (takes ownership)
    void wrap(jobject localRef) {
        release();
        if (localRef) {
            JNIEnv* env = JvmWrapper::getEnv();
            if (env) {
                m_obj = env->NewGlobalRef(localRef);
                env->DeleteLocalRef(localRef);
            }
        }
    }

private:
    jobject m_obj;
};

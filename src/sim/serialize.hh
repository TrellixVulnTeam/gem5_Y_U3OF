/*
 * Copyright (c) 2015, 2018 ARM Limited
 * All rights reserved
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 2002-2005 The Regents of The University of Michigan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Nathan Binkert
 *          Erik Hallnor
 *          Steve Reinhardt
 *          Andreas Sandberg
 */

/* @file
 * Serialization Interface Declarations
 */

#ifndef __SERIALIZE_HH__
#define __SERIALIZE_HH__


#include <algorithm>
#include <iostream>
#include <list>
#include <map>
#include <stack>
#include <set>
#include <vector>

#include "base/bitunion.hh"
#include "base/logging.hh"
#include "base/str.hh"

class IniFile;
class SimObject;
class SimObjectResolver;

typedef std::ostream CheckpointOut;

extern bool GIVE_ME_A_NAME_log_exists;
extern const char *new_home;

class CheckpointIn
{
  private:

    IniFile *db;

    SimObjectResolver &objNameResolver;

  public:
    CheckpointIn(const std::string &cpt_dir, SimObjectResolver &resolver);
    ~CheckpointIn();

    const std::string cptDir;

    bool find(const std::string &section, const std::string &entry,
              std::string &value);

    bool findObj(const std::string &section, const std::string &entry,
                 SimObject *&value);


    bool entryExists(const std::string &section, const std::string &entry);
    bool sectionExists(const std::string &section);

    // The following static functions have to do with checkpoint
    // creation rather than restoration.  This class makes a handy
    // namespace for them though.  Currently no Checkpoint object is
    // created on serialization (only unserialization) so we track the
    // directory name as a global.  It would be nice to change this
    // someday

  private:
    // current directory we're serializing into.
    static std::string currentDirectory;

  public:
    // Set the current directory.  This function takes care of
    // inserting curTick() if there's a '%d' in the argument, and
    // appends a '/' if necessary.  The final name is returned.
    static std::string setDir(const std::string &base_name);

    // Export current checkpoint directory name so other objects can
    // derive filenames from it (e.g., memory).  The return value is
    // guaranteed to end in '/' so filenames can be directly appended.
    // This function is only valid while a checkpoint is being created.
    static std::string dir();

    // Filename for base checkpoint file within directory.
    static const char *baseFilename;
};

/**
 * Basic support for object serialization.
 *
 * Objects that support serialization should derive from this
 * class. Such objects can largely be divided into two categories: 1)
 * True SimObjects (deriving from SimObject), and 2) child objects
 * (non-SimObjects).
 *
 * SimObjects are serialized automatically into their own sections
 * automatically by the SimObject base class (see
 * SimObject::serializeAll().
 *
 * SimObjects can contain other serializable objects that are not
 * SimObjects. Much like normal serialized members are not serialized
 * automatically, these objects will not be serialized automatically
 * and it is expected that the objects owning such serializable
 * objects call the required serialization/unserialization methods on
 * child objects. The preferred method to serialize a child object is
 * to call serializeSection() on the child, which serializes the
 * object into a new subsection in the current section. Another option
 * is to call serialize() directly, which serializes the object into
 * the current section. The latter is not recommended as it can lead
 * to naming clashes between objects.
 *
 * @note Many objects that support serialization need to be put in a
 * consistent state when serialization takes place. We refer to the
 * action of forcing an object into a consistent state as
 * 'draining'. Objects that need draining inherit from Drainable. See
 * Drainable for more information.
 */
class Serializable
{
  protected:
    /**
     * Scoped checkpoint section helper class
     *
     * This helper class creates a section within a checkpoint without
     * the need for a separate serializeable object. It is mainly used
     * within the Serializable class when serializing or unserializing
     * section (see serializeSection() and unserializeSection()). It
     * can also be used to maintain backwards compatibility in
     * existing code that serializes structs that are not inheriting
     * from Serializable into subsections.
     *
     * When the class is instantiated, it appends a name to the active
     * path in a checkpoint. The old path is later restored when the
     * instance is destroyed. For example, serializeSection() could be
     * implemented by instantiating a ScopedCheckpointSection and then
     * calling serialize() on an object.
     */
    class ScopedCheckpointSection {
      public:
        template<class CP>
        ScopedCheckpointSection(CP &cp, const char *name) {
            pushName(name);
            nameOut(cp);
        }

        template<class CP>
        ScopedCheckpointSection(CP &cp, const std::string &name) {
            pushName(name.c_str());
            nameOut(cp);
        }

        ~ScopedCheckpointSection();

        ScopedCheckpointSection() = delete;
        ScopedCheckpointSection(const ScopedCheckpointSection &) = delete;
        ScopedCheckpointSection &operator=(
            const ScopedCheckpointSection &) = delete;
        ScopedCheckpointSection &operator=(
            ScopedCheckpointSection &&) = delete;

      private:
        void pushName(const char *name);
        void nameOut(CheckpointOut &cp);
        void nameOut(CheckpointIn &cp) {};
    };

  public:
    Serializable();
    virtual ~Serializable();

    /**
     * Serialize an object
     *
     * Output an object's state into the current checkpoint section.
     *
     * @param cp Checkpoint state
     */
    virtual void serialize(CheckpointOut &cp) const = 0;

    /**
     * Unserialize an object
     *
     * Read an object's state from the current checkpoint section.
     *
     * @param cp Checkpoint state
     */
    virtual void unserialize(CheckpointIn &cp) = 0;

    /**
     * Serialize an object into a new section
     *
     * This method creates a new section in a checkpoint and calls
     * serialize() to serialize the current object into that
     * section. The name of the section is appended to the current
     * checkpoint path.
     *
     * @param cp Checkpoint state
     * @param name Name to append to the active path
     */
    void serializeSection(CheckpointOut &cp, const char *name) const;

    void serializeSection(CheckpointOut &cp, const std::string &name) const {
        serializeSection(cp, name.c_str());
    }

    /**
     * Unserialize an a child object
     *
     * This method loads a child object from a checkpoint. The object
     * name is appended to the active path to form a fully qualified
     * section name and unserialize() is called.
     *
     * @param cp Checkpoint state
     * @param name Name to append to the active path
     */
    void unserializeSection(CheckpointIn &cp, const char *name);

    void unserializeSection(CheckpointIn &cp, const std::string &name) {
        unserializeSection(cp, name.c_str());
    }

    /** Get the fully-qualified name of the active section */
    static const std::string &currentSection();

    static int ckptCount;
    static int ckptMaxCount;
    static int ckptPrevCount;
    static void serializeAll(const std::string &cpt_dir);
    static void unserializeGlobals(CheckpointIn &cp);

  private:
    static std::stack<std::string> path;
};

//
// The base implementations use to_number for parsing and '<<' for
// displaying, suitable for integer types.
//
template <class T>
bool
parseParam(const std::string &s, T &value)
{
    return to_number(s, value);
}

template <class T>
void
showParam(CheckpointOut &os, const T &value)
{
    os << value;
}

template <class T>
bool
parseParam(const std::string &s, BitUnionType<T> &value)
{
    // Zero initialize storage to avoid leaking an uninitialized value
    BitUnionBaseType<T> storage = BitUnionBaseType<T>();
    auto res = to_number(s, storage);
    value = storage;
    return res;
}

template <class T>
void
showParam(CheckpointOut &os, const BitUnionType<T> &value)
{
    auto storage = static_cast<BitUnionBaseType<T>>(value);

    // For a BitUnion8, the storage type is an unsigned char.
    // Since we want to serialize a number we need to cast to
    // unsigned int
    os << ((sizeof(storage) == 1) ?
        static_cast<unsigned int>(storage) : storage);
}

// Treat 8-bit ints (chars) as ints on output, not as chars
template <>
inline void
showParam(CheckpointOut &os, const char &value)
{
    os << (int)value;
}

template <>
inline void
showParam(CheckpointOut &os, const signed char &value)
{
    os << (int)value;
}

template <>
inline void
showParam(CheckpointOut &os, const unsigned char &value)
{
    os << (unsigned int)value;
}

template <>
inline bool
parseParam(const std::string &s, float &value)
{
    return to_number(s, value);
}

template <>
inline bool
parseParam(const std::string &s, double &value)
{
    return to_number(s, value);
}

template <>
inline bool
parseParam(const std::string &s, bool &value)
{
    return to_bool(s, value);
}

// Display bools as strings
template <>
inline void
showParam(CheckpointOut &os, const bool &value)
{
    os << (value ? "true" : "false");
}

// String requires no processing to speak of
template <>
inline bool
parseParam(const std::string &s, std::string &value)
{
    value = s;
    return true;
}

template <class T>
void
paramOut(CheckpointOut &os, const std::string &name, const T &param)
{
    os << name << "=";
    showParam(os, param);
    os << "\n";
}

template <class T>
void
paramIn(CheckpointIn &cp, const std::string &name, T &param)
{
    const std::string &section(Serializable::currentSection());
    std::string str;
    if (!cp.find(section, name, str) || !parseParam(str, param)) {
        fatal("Can't unserialize '%s:%s'\n", section, name);
    }
}

template <class T>
bool
optParamIn(CheckpointIn &cp, const std::string &name,
           T &param, bool warn = true)
{
    const std::string &section(Serializable::currentSection());
    std::string str;
    if (!cp.find(section, name, str) || !parseParam(str, param)) {
        if (warn)
            warn("optional parameter %s:%s not present\n", section, name);
        return false;
    } else {
        return true;
    }
}

template <class T>
void
arrayParamOut(CheckpointOut &os, const std::string &name,
              const std::vector<T> &param)
{
    typename std::vector<T>::size_type size = param.size();
    os << name << "=";
    if (size > 0)
        showParam(os, param[0]);
    for (typename std::vector<T>::size_type i = 1; i < size; ++i) {
        os << " ";
        showParam(os, param[i]);
    }
    os << "\n";
}

template <class T>
void
arrayParamOut(CheckpointOut &os, const std::string &name,
              const std::list<T> &param)
{
    typename std::list<T>::const_iterator it = param.begin();

    os << name << "=";
    if (param.size() > 0)
        showParam(os, *it);
    it++;
    while (it != param.end()) {
        os << " ";
        showParam(os, *it);
        it++;
    }
    os << "\n";
}

template <class T>
void
arrayParamOut(CheckpointOut &os, const std::string &name,
              const std::set<T> &param)
{
    typename std::set<T>::const_iterator it = param.begin();

    os << name << "=";
    if (param.size() > 0)
        showParam(os, *it);
    it++;
    while (it != param.end()) {
        os << " ";
        showParam(os, *it);
        it++;
    }
    os << "\n";
}

template <class T>
void
arrayParamOut(CheckpointOut &os, const std::string &name,
              const T *param, unsigned size)
{
    os << name << "=";
    if (size > 0)
        showParam(os, param[0]);
    for (unsigned i = 1; i < size; ++i) {
        os << " ";
        showParam(os, param[i]);
    }
    os << "\n";
}

/**
 * Extract values stored in the checkpoint, and assign them to the provided
 * array container.
 *
 * @param cp The checkpoint to be parsed.
 * @param name Name of the container.
 * @param param The array container.
 * @param size The expected number of entries to be extracted.
 */
template <class T>
void
arrayParamIn(CheckpointIn &cp, const std::string &name,
             T *param, unsigned size)
{
    const std::string &section(Serializable::currentSection());
    std::string str;
    if (!cp.find(section, name, str)) {
        fatal("Can't unserialize '%s:%s'\n", section, name);
    }

    // code below stolen from VectorParam<T>::parse().
    // it would be nice to unify these somehow...

    std::vector<std::string> tokens;

    tokenize(tokens, str, ' ');

    // Need this if we were doing a vector
    // value.resize(tokens.size());

    fatal_if(tokens.size() != size,
             "Array size mismatch on %s:%s (Got %u, expected %u)'\n",
             section, name, tokens.size(), size);

    for (std::vector<std::string>::size_type i = 0; i < tokens.size(); i++) {
        // need to parse into local variable to handle vector<bool>,
        // for which operator[] returns a special reference class
        // that's not the same as 'bool&', (since it's a packed
        // vector)
        T scalar_value;
        if (!parseParam(tokens[i], scalar_value)) {
            std::string err("could not parse \"");

            err += str;
            err += "\"";

            fatal(err);
        }

        // assign parsed value to vector
        param[i] = scalar_value;
    }
}

template <class T>
void
arrayParamIn(CheckpointIn &cp, const std::string &name, std::vector<T> &param)
{
    const std::string &section(Serializable::currentSection());
    std::string str;
    if (!cp.find(section, name, str)) {
        fatal("Can't unserialize '%s:%s'\n", section, name);
    }

    // code below stolen from VectorParam<T>::parse().
    // it would be nice to unify these somehow...

    std::vector<std::string> tokens;

    tokenize(tokens, str, ' ');

    // Need this if we were doing a vector
    // value.resize(tokens.size());

    param.resize(tokens.size());

    for (std::vector<std::string>::size_type i = 0; i < tokens.size(); i++) {
        // need to parse into local variable to handle vector<bool>,
        // for which operator[] returns a special reference class
        // that's not the same as 'bool&', (since it's a packed
        // vector)
        T scalar_value;
        if (!parseParam(tokens[i], scalar_value)) {
            std::string err("could not parse \"");

            err += str;
            err += "\"";

            fatal(err);
        }

        // assign parsed value to vector
        param[i] = scalar_value;
    }
}

template <class T>
void
arrayParamIn(CheckpointIn &cp, const std::string &name, std::list<T> &param)
{
    const std::string &section(Serializable::currentSection());
    std::string str;
    if (!cp.find(section, name, str)) {
        fatal("Can't unserialize '%s:%s'\n", section, name);
    }
    param.clear();

    std::vector<std::string> tokens;
    tokenize(tokens, str, ' ');

    for (std::vector<std::string>::size_type i = 0; i < tokens.size(); i++) {
        T scalar_value;
        if (!parseParam(tokens[i], scalar_value)) {
            std::string err("could not parse \"");

            err += str;
            err += "\"";

            fatal(err);
        }

        // assign parsed value to vector
        param.push_back(scalar_value);
    }
}

template <class T>
void
arrayParamIn(CheckpointIn &cp, const std::string &name, std::set<T> &param)
{
    const std::string &section(Serializable::currentSection());
    std::string str;
    if (!cp.find(section, name, str)) {
        fatal("Can't unserialize '%s:%s'\n", section, name);
    }
    param.clear();

    std::vector<std::string> tokens;
    tokenize(tokens, str, ' ');

    for (std::vector<std::string>::size_type i = 0; i < tokens.size(); i++) {
        T scalar_value;
        if (!parseParam(tokens[i], scalar_value)) {
            std::string err("could not parse \"");

            err += str;
            err += "\"";

            fatal(err);
        }

        // assign parsed value to vector
        param.insert(scalar_value);
    }
}

void
debug_serialize(const std::string &cpt_dir);

void
objParamIn(CheckpointIn &cp, const std::string &name, SimObject * &param);

//
// These macros are streamlined to use in serialize/unserialize
// functions.  It's assumed that serialize() has a parameter 'os' for
// the ostream, and unserialize() has parameters 'cp' and 'section'.
#define SERIALIZE_SCALAR(scalar)        paramOut(cp, #scalar, scalar)

#define UNSERIALIZE_SCALAR(scalar)      paramIn(cp, #scalar, scalar)
#define UNSERIALIZE_OPT_SCALAR(scalar)      optParamIn(cp, #scalar, scalar)

// ENUMs are like SCALARs, but we cast them to ints on the way out
#define SERIALIZE_ENUM(scalar)          paramOut(cp, #scalar, (int)scalar)

#define UNSERIALIZE_ENUM(scalar)                        \
    do {                                                \
        int tmp;                                        \
        paramIn(cp, #scalar, tmp);                      \
        scalar = static_cast<decltype(scalar)>(tmp);    \
    } while (0)

#define SERIALIZE_ARRAY(member, size)           \
        arrayParamOut(cp, #member, member, size)

#define UNSERIALIZE_ARRAY(member, size)         \
        arrayParamIn(cp, #member, member, size)

#define SERIALIZE_CONTAINER(member)             \
        arrayParamOut(cp, #member, member)

#define UNSERIALIZE_CONTAINER(member)           \
        arrayParamIn(cp, #member, member)

#define SERIALIZE_EVENT(event) event.serializeSection(cp, #event);

#define UNSERIALIZE_EVENT(event)                        \
    do {                                                \
        event.unserializeSection(cp, #event);           \
        eventQueue()->checkpointReschedule(&event);     \
    } while (0)

#define SERIALIZE_OBJ(obj) obj.serializeSection(cp, #obj)
#define UNSERIALIZE_OBJ(obj) obj.unserializeSection(cp, #obj)

#define SERIALIZE_OBJPTR(objptr)        paramOut(cp, #objptr, (objptr)->name())

#define UNSERIALIZE_OBJPTR(objptr)                      \
    do {                                                \
        SimObject *sptr;                                \
        objParamIn(cp, #objptr, sptr);                  \
        objptr = dynamic_cast<decltype(objptr)>(sptr);  \
    } while (0)

#endif // __SERIALIZE_HH__

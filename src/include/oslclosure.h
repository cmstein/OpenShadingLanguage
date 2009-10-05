/*
Copyright (c) 2009 Sony Pictures Imageworks, et al.
All Rights Reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.
* Neither the name of Sony Pictures Imageworks nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef OSLCLOSURE_H
#define OSLCLOSURE_H


#include <OpenImageIO/refcnt.h>
#include <OpenImageIO/ustring.h>


#ifdef OSL_NAMESPACE
namespace OSL_NAMESPACE {
#endif

namespace OSL {


/// Base class representation of a radiance color closure.
/// For each BSDF or emission profile, the renderer should create a
/// subclass of ClosurePrimitive and create a single object (which
/// automatically "registers" it with the system).
/// Each subclass needs to overload eval(), sample(), pdf().
class ClosurePrimitive {
public:
    /// The categories of closure primitives we can have.  It's possible
    /// to customize/extend this list as long as there is coordination
    /// between the closure primitives and the integrators.
    enum Category {
        BSDF,           ///< It's reflective and/or transmissive
        Emissive        ///< It's emissive (like a light)
    };

    ClosurePrimitive (const char *name, const char *argtypes, int category);
    virtual ~ClosurePrimitive ();

    /// Return the name of the primitive
    ///
    ustring name () const { return m_name; }

    /// Return the number of arguments that the primitive expects
    ///
    int nargs () const { return m_nargs; }

    /// Return a ustring giving the encoded argument types expected.
    /// For example, "vff" means that it takes arguments (vector, float,
    /// float).
    ustring argcodes () const { return m_argcodes; }

    /// Return the type of the i-th argument.
    ///
    TypeDesc argtype (int i) const { return m_argtypes[i]; }

    /// Return the offset (in bytes) of the i-th argument.
    ///
    int argoffset (int i) const { return m_argoffsets[i]; }

    /// How much argument memory does a primitive of this type need?
    ///
    int argmem () const { return m_argmem; }

    /// Return the category of material this primitive represents.
    ///
    int category () const { return m_category; }


    /// Assemble a primitive by name
    ///
    static const ClosurePrimitive *primitive (ustring name);

    /// Helper function: sample cosine-weighted hemisphere.
    ///
    static void sample_cos_hemisphere (const Vec3 &N, const Vec3 &omega_out,
                        float randu, float randv, Vec3 &omega_in, float &pdf);

    /// Helper function: return the PDF for cosine-weighted hemisphere.
    ///
    static float pdf_cos_hemisphere (const Vec3 &N, const Vec3 &omega_in);

private:
    ustring m_name;
    Category m_category;
    int m_nargs;
    int m_argmem;
    std::vector<TypeDesc> m_argtypes;
    std::vector<int> m_argoffsets;
    ustring m_argcodes;
};



/// Subclass of ClosurePrimitive that contains the methods needed
/// for a BSDF-like material: eval(), sample(), pdf().
class BSDFClosure : public ClosurePrimitive {
public:
    BSDFClosure (const char *name, const char *argtypes)
        : ClosurePrimitive (name, argtypes, BSDF) { }
    ~BSDFClosure () { }

    /// Return the evaluation cone -- Given instance parameters, and viewing
    /// direction omega_out (pointing away from the surface), returns the cone of
    /// directions this BSDF is sensitive to light from. If the incoming
    /// direction is in the wrong hemisphere, or if this BSDF is singular, this
    /// method should return false rather than return a degenerate cone. If this
    /// method returns true, axis must be normalized and angle must be in the
    /// range (0, 2*pi]. Note that the cone can have an angle greater than pi,
    /// this allows a surface to potentially gather light from the entire sphere
    /// of directions.
    virtual bool get_cone(const void *paramsptr,
                          const Vec3 &omega_out, Vec3 &axis, float &angle) const = 0;

    /// Evaluate the BSDF -- Given instance parameters, viewing direction omega_out
    /// and lighting direction omega_in (both pointing away from the surface),
    /// compute the amount of radiance to be transfered between these two
    /// directions.
    /// It is safe to assume that the omega_in vector is inside the cone returned
    /// above. If the get_cone method returned false, this function will never be
    /// called.
    virtual Color3 eval (const void *paramsptr,
                         const Vec3 &omega_out, const Vec3 &omega_in) const = 0;

    /// Sample the BSDF -- Given instance parameters, viewing direction omega_out
    /// (pointing away from the surface), and random deviates randu and
    /// randv on [0,1), return a sampled direction omega_in and the PDF value
    /// in that direction.
    /// Unlike the other methods, this routine can be called even if the
    /// get_cone routine returned false. This is to allow singular BRDFs to pick
    /// directions from infinitely small cones.
    virtual void sample (const void *paramsptr,
                         const Vec3 &omega_out, float randu, float randv,
                         Vec3 &omega_in, float &pdf) const = 0;

    /// Return the probability distribution function in the direction R,
    /// given the parameters and incident direction I.  This MUST match
    /// the PDF computed by sample().
    /// It is safe to assume that the omega_in vector is inside the cone returned
    /// above. If the get_cone method returned false, this function will never be
    /// called. This means that singular BSDFs should not return 1 here.
    virtual float pdf (const void *paramsptr,
                       const Vec3 &omega_out, const Vec3 &omega_in) const = 0;
};



/// Subclass of ClosurePrimitive that contains the methods needed
/// for an emissive material.
class EmissiveClosure : public ClosurePrimitive {
public:
    EmissiveClosure (const char *name, const char *argtypes)
        : ClosurePrimitive (name, argtypes, Emissive) { }
    ~EmissiveClosure () { }

    /// Evaluate the emission -- Given instance parameters, compute the
    /// outgoing radiance Er in the direction of R.  Return true if
    /// there is any (non-zero) outgoing radiance, false if there is no
    /// outgoing radiance (this allows the caller to take various
    /// shortcuts without needing to check the value of Er.  It is
    /// assumed that R is already normalized and points away from the
    /// surface position.
    virtual bool eval (const void *paramsptr, 
                       const Vec3 &R, Color3 &Er) const = 0;

    /// Sample the emission direction -- Given instance parameters and
    /// random deviates randu and randv on [0,1), return a sampled
    /// direction R and the PDF value in that direction.
    virtual void sample (const void *paramsptr, float randu, float randv,
                         Vec3 &R, float &pdf) const = 0;

    /// Return the probability distribution function in the direction R,
    /// given the parameters.  This MUST match the PDF computed by
    /// sample().
    virtual float pdf (const void *paramsptr, const Vec3 &R) const = 0;
};



/// Representation of an OSL 'closure color'.  It houses a linear
/// combination of weights * components (the components are references
/// to closure primitives and instance parameters).
class ClosureColor {
public:
    ClosureColor () { }
    ~ClosureColor () { }

    void clear () {
        m_components.clear ();
        m_mem.clear ();
    }

    void set (const ClosurePrimitive *prim, const void *params=NULL) {
        clear ();
        add_component (prim, Color3 (1.0f, 1.0f, 1.0f), params);
    }

    void add_component (const ClosurePrimitive *cprim,
                        const Color3 &weight, const void *params=NULL);
    /// *this += A
    ///
    void add (const ClosureColor &A);
    const ClosureColor & operator+= (const ClosureColor &A) {
        add (A);
        return *this;
    }

    /// *this = a+b
    ///
    void add (const ClosureColor &a, const ClosureColor &b);

#if 0
    /// *this -= A
    ///
    void sub (const ClosureColor &A);
    const ClosureColor & operator-= (const ClosureColor &A) {
        sub (A);
        return *this;
    }

    /// *this = a-b
    ///
    void sub (const ClosureColor &a, const ClosureColor &b);
#endif

    /// *this *= f
    ///
    void mul (float f);
    void mul (const Color3 &w);
    const ClosureColor & operator*= (float w) { mul(w); return *this; }
    const ClosureColor & operator*= (const Color3 &w) { mul(w); return *this; }

    /// Stream output (for debugging)
    ///
    friend std::ostream & operator<< (std::ostream &out, const ClosureColor &c);

    /// Return the number of primitive components of this closure.
    ///
    int ncomponents () const { return (int) m_components.size(); }

    /// Return the weight of the i-th primitive component of this closure.
    ///
    const Color3 & weight (int i) const { return component(i).weight; }

    /// Return a pointer to the ClosurePrimitive of the i-th primitive
    /// component of this closure.
    const ClosurePrimitive * prim (int i) const { return component(i).cprim; }

    /// Return a pointer to the raw primitive data for the i-th primitive
    /// component of this closure.
    const void *compdata (int i) const { return &m_mem[component(i).memoffset]; }

    /// Add a parameter value
    ///
    void set_parameter (int component, int param, const void *data) {
        const Component &comp (m_components[component]);
        memcpy (&m_mem[comp.memoffset+comp.cprim->argoffset(param)],
                data, comp.cprim->argtype(param).size());
    }

private:

    /// Light-weight struct to hold a single component of the Closure.
    ///
    struct Component {
        const ClosurePrimitive *cprim; ///< Which closure primitive
        int nargs;       ///< Number of arguments
        int memoffset;   ///< Offset into closure mem of our params
        Color3 weight;   ///< Weight of this component

        Component (const ClosurePrimitive *prim, const Color3 &w) 
            : cprim(prim), nargs(prim->nargs()), memoffset(0), weight(w) { }
        Component (const Component &x) : cprim(x.cprim), nargs(cprim->nargs()),
                                         memoffset(0), weight(x.weight) { }
    };

    /// Return the i-th component of this closure.
    ///
    const Component & component (int i) const { return m_components[i]; }


    std::vector<Component> m_components;   ///< The primitive components
    std::vector<char> m_mem;               ///< memory for all arguments
};




}; // namespace OSL

#ifdef OSL_NAMESPACE
}; // end namespace OSL_NAMESPACE
using namespace OSL_NAMESPACE;
#endif


#endif /* OSLCLOSURE_H */

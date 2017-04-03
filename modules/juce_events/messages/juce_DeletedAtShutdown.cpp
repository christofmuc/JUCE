/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2016 - ROLI Ltd.

   Permission is granted to use this software under the terms of the ISC license
   http://www.isc.org/downloads/software-support-policy/isc-license/

   Permission to use, copy, modify, and/or distribute this software for any
   purpose with or without fee is hereby granted, provided that the above
   copyright notice and this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH REGARD
   TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
   FITNESS. IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT,
   OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
   USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
   TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
   OF THIS SOFTWARE.

   -----------------------------------------------------------------------------

   To release a closed-source product which uses other parts of JUCE not
   licensed under the ISC terms, commercial licenses are available: visit
   www.juce.com for more information.

  ==============================================================================
*/

static SpinLock deletedAtShutdownLock; // use a spin lock because it can be statically initialised

static Array<DeletedAtShutdown*>& getDeletedAtShutdownObjects()
{
    static Array<DeletedAtShutdown*> objects;
    return objects;
}

DeletedAtShutdown::DeletedAtShutdown()
{
    const SpinLock::ScopedLockType sl (deletedAtShutdownLock);
    getDeletedAtShutdownObjects().add (this);
}

DeletedAtShutdown::~DeletedAtShutdown()
{
    const SpinLock::ScopedLockType sl (deletedAtShutdownLock);
    getDeletedAtShutdownObjects().removeFirstMatchingValue (this);
}

void DeletedAtShutdown::deleteAll()
{
    // make a local copy of the array, so it can't get into a loop if something
    // creates another DeletedAtShutdown object during its destructor.
    Array<DeletedAtShutdown*> localCopy;

    {
        const SpinLock::ScopedLockType sl (deletedAtShutdownLock);
        localCopy = getDeletedAtShutdownObjects();
    }

    for (int i = localCopy.size(); --i >= 0;)
    {
       #if JUCE_MSVC
        // Disable unreachable code warning, in case the compiler manages to figure out
        // that you have no classes of DeletedAtShutdown that could throw an exception here.
        #pragma warning (push)
        #pragma warning (disable: 4702)
       #endif

        JUCE_TRY
        {
            auto* deletee = localCopy.getUnchecked(i);

            // double-check that it's not already been deleted during another object's destructor.
            {
                const SpinLock::ScopedLockType sl (deletedAtShutdownLock);

                if (! getDeletedAtShutdownObjects().contains (deletee))
                    deletee = nullptr;
            }

            delete deletee;
        }
        JUCE_CATCH_EXCEPTION

       #if JUCE_MSVC
        #pragma warning (pop)
       #endif
    }

    // if no objects got re-created during shutdown, this should have been emptied by their
    // destructors
    jassert (getDeletedAtShutdownObjects().isEmpty());

    getDeletedAtShutdownObjects().clear(); // just to make sure the array doesn't have any memory still allocated
}

/****************************************************************************************
 
   Copyright (C) 2016 Autodesk, Inc.
   All rights reserved.
 
   Use of this software is subject to the terms of the Autodesk license agreement
   provided at the time of installation or download, or which otherwise accompanies
   this software in either electronic or hard copy form.
 
****************************************************************************************/

//! \file fbxstatus.h
#ifndef _FBXSDK_CORE_BASE_STATUS_H_
#define _FBXSDK_CORE_BASE_STATUS_H_

#include <Animation/3rdparty/fbxsdk/include/fbxsdk/fbxsdk_def.h>

#include <Animation/3rdparty/fbxsdk/include/fbxsdk/core/base/fbxstring.h>
#include <Animation/3rdparty/fbxsdk/include/fbxsdk/core/base/fbxarray.h>

#include <Animation/3rdparty/fbxsdk/include/fbxsdk/fbxsdk_nsbegin.h>

/** This class facilitates the testing/reporting of errors.  It encapsulates the
  * status code and the internal FBXSDK error code as returned by the API functions.
  * By default, the class will not accumulate all the error messages into the history array.
  * If this history is desired, the object must be configured by calling KeepErrorStringHistory(true)
  * \nosubgrouping
  */
class FBXSDK_DLL FbxStatus
{
public:


    //! Available status codes.
    enum EStatusCode {        
        eSuccess = 0,                           //!< Operation was successful
        eFailure,                               //!< Operation failed
        eInsufficientMemory,                    //!< Operation failed due to insufficient memory
        eInvalidParameter,                      //!< An invalid parameter was provided
        eIndexOutOfRange,                       //!< Index value outside the valid range
        ePasswordError,                         //!< Operation on FBX file password failed
        eInvalidFileVersion,                    //!< File version not supported (anymore or yet)
        eInvalidFile,                           //!< Operation on the file access failed
		eSceneCheckFail							//!< Scene validation failure
    };

    //! Default constructor.    
	FbxStatus();
	FbxStatus(EStatusCode pCode);
    FbxStatus(const FbxStatus& rhs);

    ~FbxStatus();

    FbxStatus&      operator=(const FbxStatus& rhs);
	FbxStatus&      operator+=(const FbxStatus& rhs);

    /** Equivalence operator.
      * \param rhs Status object to compare.
      * \return \c True if all the members of \e rhs are equal to this instance members and \c False otherwise.
      */
    bool            operator==(const FbxStatus& rhs)    const   { return (mCode == rhs.mCode); }
    /** Equivalence operator.
      * \param pCode Status code to compare.
      * \return \c True if the code member of this instance equals \e pCode and \c False otherwise.
      */
    bool            operator==(const EStatusCode pCode) const   { return (mCode == pCode); }
    /** Non-Equivalence operator.
      * \param rhs Status object to compare.
      * \return \c True if at least one member of \e rhs is not equal to this instance member and \c True otherwise.
      */
    bool            operator!=(const FbxStatus& rhs)    const   { return (mCode != rhs.mCode); }
    /** Non-Equivalence operator.
      * \param rhs Status code to compare.
      * \return \c True if the code member of this instance equals \e rhs and \c False otherwise.
      */
    bool            operator!=(const EStatusCode rhs)   const   { return (mCode != rhs); }

    /** The conversion operator that converts a FbxStatus object to bool.
      *	The result it returns will be \c True if the FbxStatus does not contain
      * an error, and \c False if it does.
      */
    operator        bool() const    { return mCode==eSuccess; }

    /** Determines whether there is an error.
      * \return \c True if an error occured and \c False if the operation was sucessful.
      */
    bool            Error() const   { return !this->operator bool(); }

    //! Clear error code and message from the instance. After this call, it will behave as if it contained eSuccess.
    void            Clear();

    //! Retrieve the type of error that occurred, as specified in the enumeration.
    EStatusCode     GetCode() const { return mCode; }

    /** Change the current code of the instance.
      * \param rhs New code value.
      */
    void            SetCode(const EStatusCode rhs);

    /** Change the current code of the instance.
      * \param rhs New code value.
      * \param pErrorMsg Optional error description string. This string can have formatting characters
      *                  The function will use the vsnprintf function to assemble the final string
      *                  using an internal buffer of 4096 characters.
      */
    void            SetCode(const EStatusCode rhs, const char* pErrorMsg, ...);

    //! Get the error message string corresponding to the current code.
    const char*     GetErrorString() const;

	/** Configure this object to remember all the error messages string.
	  * \param pState If set to true, this object will record all the error messages that can
	  *               later be access by calling GetErrorStringHistory(). If set to false,
	  *               any recorded history is also cleared.
	  * \return Previous state of the flag.
	  */
	bool            KeepErrorStringHistory(bool pState);

	/** Get the history of all the error messages that have been set.
	  * \param pHistory Array that is going to be filled with the stored string.
	  *
	  * \remark the strings are allocated with FbxNew so the pHistory array must be deleted
	  *          using the statement FbxArrayDelete<FbxString*>(pHistory).
	  * \remark the error strings are returned in stack mode (last error -> first element)
	  */
	void            GetErrorStringHistory(FbxArray<FbxString*>& pHistory);

/*****************************************************************************************************************************
** WARNING! Anything beyond these lines is for internal use, may not be documented and is subject to change without notice! **
*****************************************************************************************************************************/
#ifndef DOXYGEN_SHOULD_SKIP_THIS

private:
    EStatusCode     mCode;
    FbxString       mErrorString;
	bool            mKeepErrorStringHistory;

	FbxArray<FbxString*> mErrorStringHistory;
#endif /* !DOXYGEN_SHOULD_SKIP_THIS */
};

#include <Animation/3rdparty/fbxsdk/include/fbxsdk/fbxsdk_nsend.h>

#endif /* _FBXSDK_CORE_BASE_STATUS_H_ */

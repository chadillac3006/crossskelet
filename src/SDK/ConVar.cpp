
#include "ICvar.h"
#include "../Utils/util_sdk.h"

#include <assert.h>
#ifndef WIN32
#include <strings.h> //strcasecmp
#else
#define strcasecmp(a, b) _stricmp(a,b)
#endif

#define ALIGN_VALUE(val, alignment) ((val + alignment - 1) & ~(alignment - 1))
#define stackalloc(_size) alloca(ALIGN_VALUE(_size, 16))

ConCommandBase* ConCommandBase::s_pConCommandBases = NULL;
ConCommandBase* ConCommandBase::s_pRegisteredCommands = NULL;
IConCommandBaseAccessor* ConCommandBase::s_pAccessor = NULL;
static int s_nDLLIdentifier = -1;
static int s_nCVarFlag = 0;
static bool s_bRegistered = false;

class CDefaultAccessor : public IConCommandBaseAccessor
{
public:
    virtual bool RegisterConCommandBase(ConCommandBase* pVar)
    {
	// Link to engine's list instead
	cvar->RegisterConCommand(pVar);
	return true;
    }
};

static CDefaultAccessor s_DefaultAccessor;

//-----------------------------------------------------------------------------
// Called by the framework to register ConCommandBases with the ICVar
//-----------------------------------------------------------------------------
void ConVar_Register(int nCVarFlag, IConCommandBaseAccessor* pAccessor)
{
    if (!cvar || s_bRegistered)
	return;

    assert(s_nDLLIdentifier < 0);
    s_bRegistered = true;
    s_nCVarFlag = nCVarFlag;
    s_nDLLIdentifier = cvar->AllocateDLLIdentifier();

    ConCommandBase *pCur, *pNext;

    ConCommandBase::s_pAccessor = pAccessor ? pAccessor : &s_DefaultAccessor;
    pCur = ConCommandBase::s_pConCommandBases;

    while (pCur)
    {
	pNext = pCur->m_pNext;
	pCur->AddFlags(s_nCVarFlag);
	pCur->Init();

	ConCommandBase::s_pRegisteredCommands = pCur;

	pCur = pNext;
    }

    ConCommandBase::s_pConCommandBases = NULL;
}

void ConVar_Unregister()
{
    if (!cvar || !s_bRegistered)
	return;

    assert(s_nDLLIdentifier >= 0);
    cvar->UnregisterConCommands(s_nDLLIdentifier);
    s_nDLLIdentifier = -1;
    s_bRegistered = false;
}

ConCommandBase::ConCommandBase(void)
{
    m_bRegistered = false;
    m_pszName = NULL;
    m_pszHelpString = NULL;

    m_nFlags = 0;
    m_pNext = NULL;
}

ConCommandBase::ConCommandBase(const char* pName, const char* pHelpString /*=0*/, int flags /*= 0*/)
{
    Create(pName, pHelpString, flags);
}

ConCommandBase::~ConCommandBase(void)
{
}

bool ConCommandBase::IsCommand(void) const
{
    //	assert( 0 ); This can't assert. . causes a recursive assert in Sys_Printf, etc.
    return true;
}

CVarDLLIdentifier_t ConCommandBase::GetDLLIdentifier() const
{
    return s_nDLLIdentifier;
}

void ConCommandBase::Create(const char* pName, const char* pHelpString /*= 0*/, int flags /*= 0*/)
{
    static const char* empty_string = "";

    m_bRegistered = false;

    // Name should be static data
    m_pszName = pName;
    m_pszHelpString = pHelpString ? pHelpString : empty_string;

    m_nFlags = flags;

    if (!(m_nFlags & FCVAR_UNREGISTERED))
    {
	m_pNext = s_pConCommandBases;
	s_pConCommandBases = this;
    }
    else
    {
	m_pNext = NULL;
    }
}

void ConCommandBase::Init()
{
    if (s_pAccessor)
    {
	s_pAccessor->RegisterConCommandBase(this);
    }
}

void ConCommandBase::Shutdown()
{
    if (cvar)
    {
	cvar->UnregisterConCommand(this);
    }
}

const char* ConCommandBase::GetName(void) const
{
    return m_pszName;
}

bool ConCommandBase::IsFlagSet(int flag) const
{
    return (flag & m_nFlags) ? true : false;
}

void ConCommandBase::AddFlags(int flags)
{
    m_nFlags |= flags;
}

void ConCommandBase::RemoveFlags(int flags)
{
    m_nFlags &= ~flags;
}

int ConCommandBase::GetFlags(void) const
{
    return m_nFlags;
}

const ConCommandBase* ConCommandBase::GetNext(void) const
{
    return m_pNext;
}

ConCommandBase* ConCommandBase::GetNext(void)
{
    return m_pNext;
}

char* ConCommandBase::CopyString(const char* from)
{
    int len;
    char* to;

    len = strlen(from);
    if (len <= 0)
    {
	to = new char[1];
	to[0] = 0;
    }
    else
    {
	to = new char[len + 1];
	strncpy(to, from, len + 1);
    }
    return to;
}

const char* ConCommandBase::GetHelpText(void) const
{
    return m_pszHelpString;
}

bool ConCommandBase::IsRegistered(void) const
{
    return m_bRegistered;
}

static characterset_t s_BreakSet;
static bool s_bBuiltBreakSet = false;

void CharacterSetBuild(characterset_t* pSetBuffer, const char* pszSetString)
{
    int i = 0;

    // Test our pointers
    if (!pSetBuffer || !pszSetString)
	return;

    memset(pSetBuffer->Set, 0, sizeof(pSetBuffer->Set));

    while (pszSetString[i])
    {
	pSetBuffer->Set[pszSetString[i]] = 1;
	i++;
    }
}

CCommand::CCommand()
{
    if (!s_bBuiltBreakSet)
    {
	s_bBuiltBreakSet = true;
	CharacterSetBuild(&s_BreakSet, "{}()':");
    }

    Reset();
}

CCommand::CCommand(int nArgC, const char** ppArgV)
{
    assert(nArgC > 0);

    if (!s_bBuiltBreakSet)
    {
	s_bBuiltBreakSet = true;
	CharacterSetBuild(&s_BreakSet, "{}()':");
    }

    Reset();

    char* pBuf = m_pArgvBuffer;
    char* pSBuf = m_pArgSBuffer;
    m_nArgc = nArgC;
    for (int i = 0; i < nArgC; ++i)
    {
	m_ppArgv[i] = pBuf;
	int nLen = strlen(ppArgV[i]);
	memcpy(pBuf, ppArgV[i], nLen + 1);
	if (i == 0)
	{
	    m_nArgv0Size = nLen;
	}
	pBuf += nLen + 1;

	bool bContainsSpace = strchr(ppArgV[i], ' ') != NULL;
	if (bContainsSpace)
	{
	    *pSBuf++ = '\"';
	}
	memcpy(pSBuf, ppArgV[i], nLen);
	pSBuf += nLen;
	if (bContainsSpace)
	{
	    *pSBuf++ = '\"';
	}

	if (i != nArgC - 1)
	{
	    *pSBuf++ = ' ';
	}
    }
}

void CCommand::Reset()
{
    m_nArgc = 0;
    m_nArgv0Size = 0;
    m_pArgSBuffer[0] = 0;
}

characterset_t* CCommand::DefaultBreakSet()
{
    return &s_BreakSet;
}

bool CCommand::Tokenize(const char* pCommand, characterset_t* pBreakSet)
{
    // please, no
    return false;
}

const char* CCommand::FindArg(const char* pName) const
{
    int nArgC = ArgC();
    for (int i = 1; i < nArgC; i++)
    {
	if (!strcasecmp(Arg(i), pName))
	    return (i + 1) < nArgC ? Arg(i + 1) : "";
    }
    return 0;
}

int CCommand::FindArgInt(const char* pName, int nDefaultVal) const
{
    const char* pVal = FindArg(pName);
    if (pVal)
	return atoi(pVal);
    else
	return nDefaultVal;
}

int DefaultCompletionFunc(const char* partial, char commands[COMMAND_COMPLETION_MAXITEMS][COMMAND_COMPLETION_ITEM_LENGTH])
{
    return 0;
}

ConCommand::ConCommand(const char* pName, FnCommandCallbackV1_t callback, const char* pHelpString /*= 0*/, int flags /*= 0*/, FnCommandCompletionCallback completionFunc /*= 0*/)
{
    // Set the callback
    m_fnCommandCallbackV1 = callback;
    m_bUsingNewCommandCallback = false;
    m_bUsingCommandCallbackInterface = false;
    m_fnCompletionCallback = completionFunc ? completionFunc : DefaultCompletionFunc;
    m_bHasCompletionCallback = completionFunc != 0 ? true : false;

    // Setup the rest
    BaseClass::Create(pName, pHelpString, flags);
}

ConCommand::ConCommand(const char* pName, FnCommandCallback_t callback, const char* pHelpString /*= 0*/, int flags /*= 0*/, FnCommandCompletionCallback completionFunc /*= 0*/)
{
    // Set the callback
    m_fnCommandCallback = callback;
    m_bUsingNewCommandCallback = true;
    m_fnCompletionCallback = completionFunc ? completionFunc : DefaultCompletionFunc;
    m_bHasCompletionCallback = completionFunc != 0 ? true : false;
    m_bUsingCommandCallbackInterface = false;

    // Setup the rest
    BaseClass::Create(pName, pHelpString, flags);
}

ConCommand::ConCommand(const char* pName, ICommandCallback* pCallback, const char* pHelpString /*= 0*/, int flags /*= 0*/, ICommandCompletionCallback* pCompletionCallback /*= 0*/)
{
    // Set the callback
    m_pCommandCallback = pCallback;
    m_bUsingNewCommandCallback = false;
    m_pCommandCompletionCallback = pCompletionCallback;
    m_bHasCompletionCallback = (pCompletionCallback != 0);
    m_bUsingCommandCallbackInterface = true;

    // Setup the rest
    BaseClass::Create(pName, pHelpString, flags);
}

ConCommand::~ConCommand(void)
{
}

bool ConCommand::IsCommand(void) const
{
    return true;
}

void ConCommand::Dispatch(const CCommand& command)
{
    if (m_bUsingNewCommandCallback)
    {
	if (m_fnCommandCallback)
	{
	    (*m_fnCommandCallback)(command);
	    return;
	}
    }
    else if (m_bUsingCommandCallbackInterface)
    {
	if (m_pCommandCallback)
	{
	    m_pCommandCallback->CommandCallback(command);
	    return;
	}
    }
    else
    {
	if (m_fnCommandCallbackV1)
	{
	    (*m_fnCommandCallbackV1)();
	    return;
	}
    }

    // Command without callback!!!
    //AssertMsg(0, ("Encountered ConCommand without a callback!\n"));
}

int ConCommand::AutoCompleteSuggest(const char* partial, CUtlVector<char*>& commands)
{
    // no, valve
    return 0x1337;
}

bool ConCommand::CanAutoComplete(void)
{
    return m_bHasCompletionCallback;
}

ConVar::ConVar(const char* pName, const char* pDefaultValue, int flags /* = 0 */)
{
    Create(pName, pDefaultValue, flags);
}

ConVar::ConVar(const char* pName, const char* pDefaultValue, int flags, const char* pHelpString)
{
    Create(pName, pDefaultValue, flags, pHelpString);
}

ConVar::ConVar(const char* pName, const char* pDefaultValue, int flags, const char* pHelpString, bool bMin, float fMin, bool bMax, float fMax)
{
    Create(pName, pDefaultValue, flags, pHelpString, bMin, fMin, bMax, fMax);
}

ConVar::ConVar(const char* pName, const char* pDefaultValue, int flags, const char* pHelpString, FnChangeCallback_t callback)
{
    Create(pName, pDefaultValue, flags, pHelpString, false, 0.0, false, 0.0, callback);
}

ConVar::ConVar(const char* pName, const char* pDefaultValue, int flags, const char* pHelpString, bool bMin, float fMin, bool bMax, float fMax, FnChangeCallback_t callback)
{
    Create(pName, pDefaultValue, flags, pHelpString, bMin, fMin, bMax, fMax, callback);
}

ConVar::~ConVar(void)
{
    //if(IsRegistered())
    //    convar->UnregisterConCommand(this);
    if (m_Value.m_pszString)
    {
	delete[] m_Value.m_pszString;
	m_Value.m_pszString = NULL;
    }
}

void ConVar::InstallChangeCallback(FnChangeCallback_t callback, bool bInvoke)
{
    
}

bool ConVar::IsFlagSet(int flag) const
{
    return (flag & m_pParent->m_nFlags) ? true : false;
}

const char* ConVar::GetHelpText(void) const
{
    return m_pParent->m_pszHelpString;
}

void ConVar::AddFlags(int flags)
{
    m_pParent->m_nFlags |= flags;

#ifdef ALLOW_DEVELOPMENT_CVARS
    m_pParent->m_nFlags &= ~FCVAR_DEVELOPMENTONLY;
#endif
}

int ConVar::GetFlags(void) const
{
    return m_pParent->m_nFlags;
}

bool ConVar::IsRegistered(void) const
{
    return m_pParent->m_bRegistered;
}

const char* ConVar::GetName(void) const
{
    return m_pParent->m_pszName;
}

bool ConVar::IsCommand(void) const
{
    return false;
}

void ConVar::Init()
{
    BaseClass::Init();
}

const char* ConVar::GetBaseName(void) const
{
    return m_pParent->m_pszName;
}

int ConVar::GetSplitScreenPlayerSlot(void) const
{
    return 0;
}

void ConVar::InternalSetValue(const char* value)
{
    float fNewValue;
    char tempVal[32];
    char* val;

    auto temp = *(uint32_t*)&m_Value.m_fValue ^ (uint32_t)this;
    float flOldValue = *(float*)(&temp);

    val = (char*)value;
    fNewValue = (float)atof(value);

    if (ClampValue(fNewValue))
    {
	snprintf(tempVal, sizeof(tempVal), "%f", fNewValue);
	val = tempVal;
    }

    // Redetermine value
    *(uint32_t*)&m_Value.m_fValue = *(uint32_t*)&fNewValue ^ (uint32_t)this;
    *(uint32_t*)&m_Value.m_nValue = (uint32_t)fNewValue ^ (uint32_t)this;

    if (!(m_nFlags & FCVAR_NEVER_AS_STRING))
    {
	ChangeStringValue(val, flOldValue);
    }
}

void ConVar::ChangeStringValue(const char* tempVal, float flOldValue)
{
    char* pszOldValue = (char*)stackalloc(m_Value.m_StringLength);
    memcpy(pszOldValue, m_Value.m_pszString, m_Value.m_StringLength);

    int len = strlen(tempVal) + 1;

    if (len > m_Value.m_StringLength)
    {
	if (m_Value.m_pszString)
	{
	    delete[] m_Value.m_pszString;
	}

	m_Value.m_pszString = new char[len];
	m_Value.m_StringLength = std::to_string(this->GetFloat()).size()+1;
    }

    memcpy(m_Value.m_pszString, std::to_string(this->GetFloat()).c_str(), len);

    // Invoke any necessary callback function
    for (int i = 0; i < m_fnChangeCallbacks.Count(); i++)
    {
	m_fnChangeCallbacks[i](this, pszOldValue, flOldValue);
    }

    if (cvar)
	cvar->CallGlobalChangeCallbacks(this, pszOldValue, flOldValue);
}

bool ConVar::ClampValue(float& value)
{
    if (m_bHasMin && (value < m_fMinVal))
    {
	value = m_fMinVal;
	return true;
    }

    if (m_bHasMax && (value > m_fMaxVal))
    {
	value = m_fMaxVal;
	return true;
    }

    return false;
}

void ConVar::InternalSetFloatValue(float fNewValue)
{
    if (fNewValue == m_Value.m_fValue)
	return;

    ClampValue(fNewValue);

    // Redetermine value
    float flOldValue = m_Value.m_fValue;
    *(uint32_t*)&m_Value.m_fValue = *(uint32_t*)&fNewValue ^ (uint32_t)this;
    *(uint32_t*)&m_Value.m_nValue = (uint32_t)fNewValue ^ (uint32_t)this;

    if (!(m_nFlags & FCVAR_NEVER_AS_STRING))
    {
	char tempVal[32];
	snprintf(tempVal, sizeof(tempVal), "%f", m_Value.m_fValue);
	ChangeStringValue(tempVal, flOldValue);
    }
    else
    {
	//assert(m_fnChangeCallbacks.Count() == 0);
    }
}

void ConVar::InternalSetIntValue(int nValue)
{
    if (nValue == ((int)m_Value.m_nValue ^ (int)this))
	return;

    float fValue = (float)nValue;
    if (ClampValue(fValue))
    {
	nValue = (int)(fValue);
    }

    // Redetermine value
    float flOldValue = m_Value.m_fValue;
    *(uint32_t*)&m_Value.m_fValue = *(uint32_t*)&fValue ^ (uint32_t)this;
    *(uint32_t*)&m_Value.m_nValue = *(uint32_t*)&nValue ^ (uint32_t)this;

    if (!(m_nFlags & FCVAR_NEVER_AS_STRING))
    {
	char tempVal[32];
	snprintf(tempVal, sizeof(tempVal), "%d", m_Value.m_nValue);
	ChangeStringValue(tempVal, flOldValue);
    }
    else
    {
	//assert(m_fnChangeCallbacks.Count() == 0);
    }
}

void ConVar::InternalSetColorValue(Color cValue)
{
    //int color = (int)cValue.;
    //InternalSetIntValue(color);
}

void ConVar::Create(const char* pName, const char* pDefaultValue, int flags /*= 0*/,
    const char* pHelpString /*= NULL*/, bool bMin /*= false*/, float fMin /*= 0.0*/,
    bool bMax /*= false*/, float fMax /*= false*/, FnChangeCallback_t callback /*= NULL*/)
{
    static const char* empty_string = "";

    m_pParent = this;

    // Name should be static data
    m_pszDefaultValue = pDefaultValue ? pDefaultValue : empty_string;

    m_Value.m_StringLength = strlen(m_pszDefaultValue) + 1;
    m_Value.m_pszString = new char[m_Value.m_StringLength];
    memcpy(m_Value.m_pszString, m_pszDefaultValue, m_Value.m_StringLength);

    m_bHasMin = bMin;
    m_fMinVal = fMin;
    m_bHasMax = bMax;
    m_fMaxVal = fMax;

    if (callback)
	m_fnChangeCallbacks.AddToTail(callback);

    float value = (float)atof(m_Value.m_pszString);

    *(uint32_t*)&m_Value.m_fValue = *(uint32_t*)&value ^ (uint32_t)this;
    *(uint32_t*)&m_Value.m_nValue = *(uint32_t*)&value ^ (uint32_t)this;

    BaseClass::Create(pName, pHelpString, flags);
}

void ConVar::SetValue(const char* value)
{
    ConVar* var = (ConVar*)m_pParent;
    var->InternalSetValue(value);
}

void ConVar::SetValue(float value)
{
    ConVar* var = (ConVar*)m_pParent;
    var->InternalSetFloatValue(value);
}

void ConVar::SetValue(int value)
{
    ConVar* var = (ConVar*)m_pParent;
    var->InternalSetIntValue(value);
}

void ConVar::SetValue(Color value)
{
    ConVar* var = (ConVar*)m_pParent;
    var->InternalSetColorValue(value);
}

void ConVar::Revert(void)
{
    // Force default value again
    ConVar* var = (ConVar*)m_pParent;
    var->SetValue(var->m_pszDefaultValue);
}

bool ConVar::GetMin(float& minVal) const
{
    minVal = m_pParent->m_fMinVal;
    return m_pParent->m_bHasMin;
}

bool ConVar::GetMax(float& maxVal) const
{
    maxVal = m_pParent->m_fMaxVal;
    return m_pParent->m_bHasMax;
}

const char* ConVar::GetDefault(void) const
{
    return m_pParent->m_pszDefaultValue;
}

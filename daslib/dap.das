options indenting = 4
options no_unused_block_arguments = false
options no_unused_function_arguments = false
module dap shared

require daslib/json
require daslib/json_boost
require strings


// https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Initialize
struct InitializeRequestArguments {}

def InitializeRequestArguments(data: JsonValue?)
    return [[InitializeRequestArguments]]


// https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Disconnect
struct DisconnectArguments
    restart: bool
    terminateDebuggee: bool
    suspendDebuggee: bool

def DisconnectArguments(data: JsonValue?)
    return [[DisconnectArguments restart=job(data, "restart"),
                                 terminateDebuggee=job(data, "terminateDebuggee"),
                                 suspendDebuggee=job(data, "suspendDebuggee")
            ]]


// https://microsoft.github.io/debug-adapter-protocol/specification#Types_Capabilities
struct Capabilities
    supportsConfigurationDoneRequest: bool
    supportsRestartRequest: bool
    supportTerminateDebuggee: bool
    supportsTerminateRequest: bool
    supportsExceptionOptions: bool
    supportsExceptionFilterOptions: bool
    supportsDelayedStackTraceLoading: bool


// https://microsoft.github.io/debug-adapter-protocol/specification#Requests_SetBreakpoints
struct SetBreakpointsArguments
    source: Source
    breakpoints: array<SourceBreakpoint>
    sourceModified: bool

def SetBreakpointsArguments(data: JsonValue?)
    var res <- [[SetBreakpointsArguments source=Source(joj(data, "source")),
                                         sourceModified=job(data, "sourceModified") ]]
    var breakpoints = joj(data, "breakpoints")
    if breakpoints != null
        for it in breakpoints.value as _array
            res.breakpoints |> emplace(SourceBreakpoint(it))
    return <- res


// https://microsoft.github.io/debug-adapter-protocol/specification#Types_Source
struct Source
    name: string
    path: string

def Source(data: JsonValue?)
    return [[Source name=jos(data, "name"), path=jos(data, "path") ]]


// https://microsoft.github.io/debug-adapter-protocol/specification#Types_SourceBreakpoint
struct SourceBreakpoint
    line: double

def SourceBreakpoint(data: JsonValue?)
    return [[SourceBreakpoint line=jon(data, "line") ]]


// https://microsoft.github.io/debug-adapter-protocol/specification#Requests_SetBreakpoints
struct SetBreakpointsResponse
    breakpoints: array<Breakpoint>


// https://microsoft.github.io/debug-adapter-protocol/specification#Types_Breakpoint
struct Breakpoint
    id: double
    verified: bool
    source: Source
    line: double
    message: string


// https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Threads
struct ThreadsResponseBody
    threads: array<Thread>


// https://microsoft.github.io/debug-adapter-protocol/specification#Types_Thread
struct Thread
    id: double
    name: string


// https://microsoft.github.io/debug-adapter-protocol/specification#Requests_StackTrace
struct StackTraceArguments
    threadId: double
    startFrame: double
    levels: double

def StackTraceArguments(data: JsonValue?)
    return <- [[StackTraceArguments threadId=jon(data, "threadId"), startFrame=jon(data, "startFrame"), levels=jon(data, "levels")]]

struct StackTraceResponseBody
    stackFrames: array<StackFrame>
    totalFrames: double

struct StackFrame
    id: double
    name: string
    source: Source
    line: double
    column: double


// https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Scopes
struct ScopesArguments
    frameId: double

def ScopesArguments(data: JsonValue?)
    return <- [[ScopesArguments frameId=jon(data, "frameId") ]]

struct ScopesResponseBody
    scopes: array<Scope>


// https://microsoft.github.io/debug-adapter-protocol/specification#Types_Scope
struct Scope
    name: string
    variablesReference: double


// https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Variables
struct VariablesArguments
    variablesReference: double

def VariablesArguments(data: JsonValue?)
    return <- [[VariablesArguments variablesReference=jon(data, "variablesReference") ]]

struct VariablesResponseBody
    variables: array<Variable>


// https://microsoft.github.io/debug-adapter-protocol/specification#Types_Variable
struct Variable
    name: string
    value: string
    variablesReference: double

// https://microsoft.github.io/debug-adapter-protocol/specification#Events_Output
struct OutputEventBody
    category: string
    output: string


// https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Continue
struct ContinueArguments
    threadId: double

def ContinueArguments(data: JsonValue?)
    return <- [[ContinueArguments threadId=jon(data, "threadId") ]]


// https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Pause
struct PauseArguments
    threadId: double

def PauseArguments(data: JsonValue?)
    return <- [[PauseArguments threadId=jon(data, "threadId") ]]


// https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Pause
struct StepInArguments
    threadId: double

def StepInArguments(data: JsonValue?)
    return <- [[StepInArguments threadId=jon(data, "threadId") ]]


// https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Next
struct NextArguments
    threadId: double

def NextArguments(data: JsonValue?)
    return <- [[NextArguments threadId=jon(data, "threadId") ]]


// https://microsoft.github.io/debug-adapter-protocol/specification#Requests_StepOut
struct StepOutArguments
    threadId: double

def StepOutArguments(data: JsonValue?)
    return <- [[StepOutArguments threadId=jon(data, "threadId") ]]




def joj(val : JsonValue?; id : string) : JsonValue?
    var res : JsonValue? = null
    if val == null || !(val is _object)
        return res
    find_if_exists(val as _object, id) <| $(v)
        res = *v
    return res

def jon(val : JsonValue?; id : string; defVal = 0lf) : double
    var res = defVal
    if val == null || !(val is _object)
        return res
    find_if_exists(val.value as _object, id) <| $(v)
        if v != null && (*v).value is _number
            res = (*v).value as _number
    return res

def j_s(val : JsonValue?; defVal = "") : string
    return val?.value ?as _string ?? defVal

def jos(val : JsonValue?; id : string; defVal = "") : string
    var res = defVal
    if val == null || !(val is _object)
        return res
    find_if_exists(val.value as _object, id) <| $(v)
        if v != null && (*v).value is _string
            res = (*v).value as _string
    return res

def job(val : JsonValue?; id : string; defVal = false) : bool
    var res = defVal
    if val == null || !(val is _object)
        return res
    find_if_exists(val.value as _object, id) <| $(v)
        if v != null && (*v).value is _bool
            res = (*v).value as _bool
    return res
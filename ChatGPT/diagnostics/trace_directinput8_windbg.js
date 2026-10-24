/*
 * SpiderMan2k1Fix - external debugger DirectInput8 observation prototype.
 * Requires 32-bit CDB/WinDbg + JavaScript debugger provider on Windows.
 * Does NOT patch the game's COM vtables, call Acquire, or synthesize input.
 * All bp commands only install debugger-owned software breakpoints.
 * This is a diagnostic experiment, not a gameplay fix.
 */
"use strict";
var di8 = {
    factory: 0, devices: {}, methods: {}, contexts: {}, nextToken: 1,
    logReads: 0, errors: 0
};

function di8Hex(x) { return "0x" + (x >>> 0).toString(16); }
function di8Num(x) {
    return (typeof x === "number" ? x : x.asNumber()) >>> 0;
}
function di8Read(address) {
    return di8Num(host.memory.readMemoryValues(address >>> 0, 1, 4)[0]);
}
function di8Reg(name) { return di8Num(host.evaluateExpression("@" + name)); }
function di8Log(message) {
    host.diagnostics.debugLog("SPIDEY-DI8: ", message, "\n");
}
function di8Command(c) {
    host.namespace.Debugger.Utility.Control.ExecuteCommand(c);
}
function di8Break(address, js) {
    var cmd = "bp " + di8Hex(address) + " \"dx @$scriptContents." + js + "; gc\"";
    di8Command(cmd);
}
function di8Return(context) {
    // Every return breakpoint applies to this x86 thread only, and fires once.
    var esp = di8Reg("esp");
    var addr = di8Read(esp);
    var tok = di8.nextToken++;
    di8.contexts[tok] = context;
    di8Command("~.bp /1 " + di8Hex(addr) +
        " \"dx @$scriptContents.di8OnReturn(" + tok + "); gc\"");
}
function di8WatchMethod(ptr, slot, kind) {
    var vt = di8Read(ptr);
    var method = di8Read(vt + slot * 4);
    var key = kind + "@" + di8Hex(method);
    if (!method || di8.methods[key]) return;
    di8.methods[key] = true;
    if (kind === "CreateDevice") di8Break(method, "di8OnCreateDevice()");
    else di8Break(method, "di8OnMethod(" + slot + ")");
    di8Log("watch " + kind + " native=" + di8Hex(method));
}
function di8WatchDevice(ptr, guidFirstDword) {
    var label = guidFirstDword === 0x6f1d2b61 ? "keyboard" :
        (guidFirstDword === 0x6f1d2b60 ? "mouse" : "other");
    if (!ptr || di8.devices[ptr]) return;
    di8.devices[ptr] = { label: label, Acquire: 0, Unacquire: 0,
        GetDeviceState: 0, GetDeviceData: 0, last: {} };
    di8Log("native device " + label + "=" + di8Hex(ptr));
    di8WatchMethod(ptr, 7, "Acquire");
    di8WatchMethod(ptr, 8, "Unacquire");
    di8WatchMethod(ptr, 9, "GetDeviceState");
    di8WatchMethod(ptr, 10, "GetDeviceData");
}
function di8OnFactory() {
    try {
        var esp = di8Reg("esp");
        var out = di8Read(esp + 16);
        di8.factory++;
        di8Return({ type: "factory", out: out });
    } catch (e) { di8Log("factory observer error: " + e); }
}
function di8OnCreateDevice() {
    try {
        var esp = di8Reg("esp");
        var self = di8Read(esp + 4);
        var guid = di8Read(esp + 8);
        var out = di8Read(esp + 12);
        var guidFirst = guid ? di8Read(guid) : 0;
        di8Return({ type: "device", self: self, guid: guidFirst, out: out });
    } catch (e) { di8Log("CreateDevice observer error: " + e); }
}
function di8OnMethod(slot) {
    var kind = { 7: "Acquire", 8: "Unacquire", 9: "GetDeviceState", 10: "GetDeviceData" }[slot];
    if (!kind) return;
    try {
        var esp = di8Reg("esp");
        var self = di8Read(esp + 4);
        var device = di8.devices[self];
        if (!device) return;
        var num = ++device[kind];
        di8Return({ type: "call", ptr: self, kind: kind, num: num });
    } catch (e) { di8Log(kind + " observer error: " + e); }
}
function di8OnReturn(token) {
    try {
        var context = di8.contexts[token];
        delete di8.contexts[token];
        if (!context) return;
        var hr = di8Reg("eax");
        if (context.type === "factory") {
            var interfacePtr = (hr & 0x80000000) || !context.out ? 0 : di8Read(context.out);
            di8Log("DirectInput8Create hr=" + di8Hex(hr) +
                " interface=" + di8Hex(interfacePtr));
            if (interfacePtr) di8WatchMethod(interfacePtr, 3, "CreateDevice");
            return;
        }
        if (context.type === "device") {
            var devicePtr = (hr & 0x80000000) || !context.out ? 0 : di8Read(context.out);
            di8Log("CreateDevice guid0=" + di8Hex(context.guid) +
                " hr=" + di8Hex(hr) + " device=" + di8Hex(devicePtr));
            if (devicePtr) di8WatchDevice(devicePtr, context.guid);
            return;
        }
        var device = di8.devices[context.ptr];
        if (!device) return;
        var prev = device.last[context.kind];
        device.last[context.kind] = hr;
        if (context.num <= 4 || context.num % 300 === 0 || prev !== hr || (hr & 0x80000000)) {
            if (!(hr & 0x80000000) || context.num <= 12 || context.num % 100 === 0 || prev !== hr)
                di8Log(device.label + " " + context.kind +
                    " count=" + context.num + " hr=" + di8Hex(hr) +
                    " object=" + di8Hex(context.ptr));
        }
        if (hr & 0x80000000) di8.errors++;
        di8.logReads++;
    } catch (e) { di8Log("return observer error: " + e); }
}
function di8Start() {
    di8Command("bu dinput8!DirectInput8Create \"dx @$scriptContents.di8OnFactory(); gc\"");
    di8Log("external observer armed: x86 DirectInput8Create; no DLL/vtable changes");
    di8Log("If native code paths do not use this export, factory count stays at zero.");
}
function di8Summary() {
    di8Log("factory-calls=" + di8.factory + " read-results=" + di8.logReads +
        " failing-results=" + di8.errors);
    for (var key in di8.devices) {
        var d = di8.devices[key];
        di8Log(d.label + " ptr=" + di8Hex(Number(key)) +
            " Acquire=" + d.Acquire + " Unacquire=" + d.Unacquire +
            " GetDeviceState=" + d.GetDeviceState + " GetDeviceData=" + d.GetDeviceData +
            " lastGetDeviceState=" + di8Hex(d.last.GetDeviceState || 0) +
            " lastGetDeviceData=" + di8Hex(d.last.GetDeviceData || 0));
    }
}
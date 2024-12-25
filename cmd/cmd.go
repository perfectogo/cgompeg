package cmd

/*
#cgo LDFLAGS: -lavformat -lavcodec -lswscale -lavutil
#include "./../core/cgompeg.h"
#include "./../core/cgompeg.c"
*/
import "C"
import "unsafe"

func Cmd(inputFile, outputFile string) int {

	cInputFile := C.CString(inputFile)
	cOutputFile := C.CString(outputFile)

	defer C.free(unsafe.Pointer(cInputFile))
	defer C.free(unsafe.Pointer(cOutputFile))

	return int(C.cmd(cInputFile, cOutputFile))
}


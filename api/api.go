package api

/*
#cgo LDFLAGS: -lavformat -lavcodec -lswscale -lavutil
#include "./stream/cgompeg.h"
#include "./stream/cgompeg.c"
*/
import "C"
import (
	"bytes"
	"io"
	"net/http"
	"unsafe"

	"github.com/labstack/echo/v4"
	echoSwagger "github.com/swaggo/echo-swagger"
)

// @title Go and C Code Interoperability API
// @version 1.0
// @description This is a sample API for Go and C code interoperability.
// @host localhost:8080
// @BasePath /
func Api() {
	e := echo.New()
	e.POST("/upload", upload)
	e.GET("/swagger/*", echoSwagger.WrapHandler)
	e.Start(":8080")
}

// upload is the handler for the upload endpoint
// it receives a file from the client and writes it to a pipe
// the C function then reads the file from the pipe and writes it to a file

// godoc upload
// @Summary Upload a file
// @Description Upload a file to the server
// @Accept multipart/form-data
// @Param file formData file true "The file to upload"
// @Success 200 {string} string "File processed and saved by C!"
// @Failure 400 {string} string "Failed to retrieve file"
// @Failure 500 {string} string "Failed to open uploaded file"
// @Failure 500 {string} string "Failed to create pipe"
// @Failure 500 {string} string "Failed to read from pipe"
// @Router /upload [post]
func upload(c echo.Context) error {
	file, err := c.FormFile("file")
	if err != nil {
		return c.String(http.StatusBadRequest, "Failed to get file")
	}

	src, err := file.Open()
	if err != nil {
		return c.String(http.StatusInternalServerError, "Failed to open file")
	}
	defer src.Close()

	// Read entire file into memory
	buf := new(bytes.Buffer)
	if _, err := io.Copy(buf, src); err != nil {
		return c.String(http.StatusInternalServerError, "Failed to read file")
	}

	// Get the buffer as byte slice
	data := buf.Bytes()

	// Pass the buffer to C
	result := C.stream_to_hls(
		(*C.uchar)(unsafe.Pointer(&data[0])),
		C.size_t(len(data)),
	)

	if result != 0 {
		return c.String(http.StatusInternalServerError, "Failed to process video")
	}

	return c.String(http.StatusOK, "Stream processed successfully!")
}

// swagg command: swag init -d . -o ./docs --parseDependency
// swagg url: localhost:8080/swagger/index.html

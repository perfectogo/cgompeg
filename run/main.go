package main

/*
#include "ccode/main.h"
#include "ccode/main.c"
*/
import "C"
import (
	"io"
	"net/http"
	"os"

	_ "app/docs"

	"github.com/labstack/echo/v4"
	echoSwagger "github.com/swaggo/echo-swagger"
)

// @title Go and C Code Interoperability API
// @version 1.0
// @description This is a sample API for Go and C code interoperability.
// @host localhost:8080
// @BasePath /
func main() {
	router := echo.New()
	router.POST("/upload", upload)

	// swagger
	router.GET("/swagger/*", echoSwagger.WrapHandler)
	router.Start(":8080")
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
		return c.String(http.StatusBadRequest, "Failed to retrieve file: "+err.Error())
	}

	src, err := file.Open()
	// view size of file

	if err != nil {
		return c.String(http.StatusInternalServerError, "Failed to open uploaded file: "+err.Error())
	}
	defer src.Close()

	rPipe, wPipe, err := os.Pipe()
	if err != nil {
		return c.String(http.StatusInternalServerError, "Failed to create pipe: "+err.Error())
	}

	go func() {
		defer wPipe.Close()
		io.Copy(wPipe, src)
	}()

	code := C.read_file_from_pipe(C.int(rPipe.Fd()))
	if code != 0 {
		return c.String(http.StatusInternalServerError, "Failed to read from pipe: ")
	}

	rPipe.Close()

	return c.String(http.StatusOK, "File processed and saved by C!")
}

// swagg command: swag init -d . -o ./docs --parseDependency
// swagg url: localhost:8080/swagger/index.html

package api

import (
	"net/http"

	"github.com/labstack/echo/v4"
)

func Upload(c echo.Context) error {
	return c.String(http.StatusOK, "Hello, World!")
}

func Api() {
	router := echo.New()

	router.POST("/upload", Upload)

	router.Start(":8080")
}

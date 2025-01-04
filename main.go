package main

import (
	"log"

	"github.com/perfectogo/cgompeg/api"
)

// func main() {

// 	start := time.Now()
// 	if len(os.Args) < 2 {
// 		log.Fatalln(errors.New("not enough arguments"))
// 	}

// 	res := cmd.Cmd(os.Args[1], os.Args[2])

// 	if res != 0 {
// 		log.Fatalln(errors.New("HLS conversion failed"))
// 	}

// 	elapsed := time.Since(start)
// 	log.Printf("Time taken: %s", elapsed)
// }

// @title Go and C Code Interoperability API
// @version 1.0
// @description This is a sample API for Go and C code interoperability.
// @host localhost:8080
// @BasePath /
func main() {
	if err := api.StartServer(":8080"); err != nil {
		log.Fatal(err)
	}
}

package main

import (
	"errors"
	"log"
	"os"

	"github.com/perfectogo/cgompeg/cmd"
)

func main() {

	if len(os.Args) < 2 {
		log.Fatalln(errors.New("not enough arguments"))
	}

	res := cmd.Cmd(os.Args[1], os.Args[2])

	if res != 0 {
		log.Fatalln(errors.New("HLS conversion failed"))
	}
}

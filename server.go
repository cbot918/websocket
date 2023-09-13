package main

import (
	"fmt"
	"log"
	"net/http"

	"github.com/gorilla/websocket"
)

const (
	HTMLPath = "./index.html"
	port     = ":8080"
)

type Con struct {
	Id   int
	Name string
	Conn *websocket.Conn
}

var cons []Con
var index int
var lg = fmt.Println

func emit(room string, mt int, m []byte) {
	for _, con := range cons {
		if err := con.Conn.WriteMessage(mt, m); err != nil {
			lg(err)
			return
		}
	}
}

func handleWS(w http.ResponseWriter, r *http.Request) {

	newCon := Con{
		Id:   index,
		Name: "default",
	}
	cons = append(cons, newCon)

	u := &websocket.Upgrader{}
	conn, err := u.Upgrade(w, r, nil)
	if err != nil {
		lg("upgrade failed")
		return
	}
	cons[index].Conn = conn

	index += 1
	for {
		mt, m, err := conn.ReadMessage()
		if err != nil {
			continue
		} else {
			lg("recv: " + string(m))
			emit("default", mt, m)
		}
	}

}

func home(w http.ResponseWriter, r *http.Request) {
	http.ServeFile(w, r, HTMLPath)
}

func main() {

	index = 0
	http.HandleFunc("/", home)

	http.HandleFunc("/ws", handleWS)

	fmt.Println("listening " + port)
	if err := http.ListenAndServe(port, nil); err != nil {
		log.Fatal(err)
	}
}

import 'package:flutter/material.dart';

class MainWindow extends StatelessWidget {
  const MainWindow({super.key});

  @override
  Widget build(BuildContext context) {
    return SizedBox(
      height: 240,
      width: 320,
      child: Stack(
        children: [
          Container(
            color: Color.fromARGB(255, 1, 36, 1),
            height: 240,
            width: 320,
            child: Stack(
              children: [
                Text(
                  "", 
                  textAlign: TextAlign.center,
                  textScaler: TextScaler.linear(1),
                  style: TextStyle(
                    color: Color.fromARGB(255, 234, 195, 195),
                    fontFamily: "源ノ角ゴシック JP",
                    fontSize: 50,
                    fontVariations: [FontVariation.weight(600)],
                    height: 1,
                  ),
                ),
                Positioned(
                  height: 82,
                  left: 2,
                  top: 10,
                  width: 160,
                  child: Text(
                    "文字列中に\n改行", 
                    textAlign: TextAlign.center,
                    textScaler: TextScaler.linear(1),
                    style: TextStyle(
                      color: Color.fromARGB(255, 234, 195, 195),
                      fontFamily: "源ノ角ゴシック JP",
                      fontSize: 30,
                      fontVariations: [FontVariation.weight(600)],
                      height: 1,
                    ),
                  ),
                ),
                Positioned(
                  height: 82,
                  left: 169,
                  top: 10,
                  width: 144,
                  child: Column(
                    children: [
                      Row(
                        children: [
                          Text(
                            "文字列", 
                            textAlign: TextAlign.center,
                            textScaler: TextScaler.linear(1),
                            style: TextStyle(
                              color: Color.fromARGB(255, 234, 195, 195),
                              fontFamily: "KozGoPr6N-Regular",
                              fontSize: 30,
                              fontVariations: [FontVariation.weight(600)],
                              height: 1,
                            ),
                          ),
                          Text(
                            "中", 
                            textAlign: TextAlign.center,
                            textScaler: TextScaler.linear(1),
                            style: TextStyle(
                              color: Color.fromARGB(255, 234, 195, 195),
                              fontFamily: "KozGoPr6N-Regular",
                              fontSize: 16,
                              fontVariations: [FontVariation.weight(600)],
                              height: 1,
                            ),
                          ),
                          Text(
                            "に", 
                            textAlign: TextAlign.center,
                            textScaler: TextScaler.linear(1),
                            style: TextStyle(
                              color: Color.fromARGB(255, 234, 195, 195),
                              fontFamily: "KozGoPr6N-Regular",
                              fontSize: 30,
                              fontVariations: [FontVariation.weight(600)],
                              height: 1,
                            ),
                          ),
                        ],
                      ),
                      Row(
                        children: [
                          Text(
                            "別", 
                            textAlign: TextAlign.center,
                            textScaler: TextScaler.linear(1),
                            style: TextStyle(
                              color: Color.fromARGB(255, 234, 195, 195),
                              fontFamily: "KozGoPr6N-Regular",
                              fontSize: 30,
                              fontVariations: [FontVariation.weight(600)],
                              height: 1,
                            ),
                          ),
                          Text(
                            "フォント", 
                            textAlign: TextAlign.center,
                            textScaler: TextScaler.linear(1),
                            style: TextStyle(
                              color: Color.fromARGB(255, 234, 195, 195),
                              fontFamily: "源ノ角ゴシック JP",
                              fontSize: 18,
                              fontVariations: [FontVariation.weight(600)],
                              height: 1,
                            ),
                          ),
                        ],
                      ),
                    ],
                  ),
                ),
                Positioned(
                  height: 82,
                  left: 25,
                  top: 110,
                  width: 86,
                  child: Text(
                    "Shift\n+改行", 
                    textAlign: TextAlign.center,
                    textScaler: TextScaler.linear(1),
                    style: TextStyle(
                      color: Color.fromARGB(255, 234, 195, 195),
                      fontFamily: "KozGoPr6N-Regular",
                      fontSize: 30,
                      fontVariations: [FontVariation.weight(600)],
                      height: 1,
                    ),
                  ),
                ),
                Positioned(
                  height: 116,
                  left: 160,
                  top: 110,
                  width: 158,
                  child: Text(
                    "段落テキストは折り返される", 
                    textAlign: TextAlign.center,
                    textScaler: TextScaler.linear(1),
                    style: TextStyle(
                      color: Color.fromARGB(255, 234, 195, 195),
                      fontFamily: "KozGoPr6N-Regular",
                      fontSize: 24,
                      fontVariations: [FontVariation.weight(600)],
                      height: 1,
                    ),
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

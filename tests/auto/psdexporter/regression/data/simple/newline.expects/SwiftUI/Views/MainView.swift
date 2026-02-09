import SwiftUI

struct MainView: View {
    var body: some View {
        ZStack {
            Group {
                Rectangle
                    .fill(Color(red: 0.004, green: 0.141, blue: 0.004))
                    .frame(width: 320.0, height: 240.0)
                    .position(x: 160.0, y: 120.0)
                Text("")
                    .font(.custom("Source Han Sans", size: 50))
                    .foregroundStyle(Color(red: 0.918, green: 0.763, blue: 0.763))
                    .multilineTextAlignment(.center)
                    .frame(width: 0.0, height: 97.0)
                    .position(x: 38.0, y: 92.5)
                Text("文字列中に\n改行")
                    .font(.custom("Source Han Sans", size: 30))
                    .foregroundStyle(Color(red: 0.918, green: 0.763, blue: 0.763))
                    .multilineTextAlignment(.center)
                    .frame(width: 160.0, height: 110.0)
                    .position(x: 82.0, y: 65.0)
                VStack(alignment: .leading, spacing: 0) {
                    HStack(spacing: 0) {
                        Text("文字列")
                            .font(.custom("Koz Go Pr6N", size: 30))
                            .foregroundStyle(Color(red: 0.918, green: 0.763, blue: 0.763))
                        Text("中")
                            .font(.custom("Koz Go Pr6N", size: 16))
                            .foregroundStyle(Color(red: 0.918, green: 0.763, blue: 0.763))
                        Text("に")
                            .font(.custom("Koz Go Pr6N", size: 30))
                            .foregroundStyle(Color(red: 0.918, green: 0.763, blue: 0.763))
                    }
                    HStack(spacing: 0) {
                        Text("別")
                            .font(.custom("Koz Go Pr6N", size: 30))
                            .foregroundStyle(Color(red: 0.918, green: 0.763, blue: 0.763))
                        Text("フォント")
                            .font(.custom("Source Han Sans", size: 18))
                            .foregroundStyle(Color(red: 0.918, green: 0.763, blue: 0.763))
                    }
                }
                    .frame(width: 144.0, height: 98.0)
                    .position(x: 241.0, y: 59.0)
                Text("Shift\n+改行")
                    .font(.custom("Koz Go Pr6N", size: 30))
                    .foregroundStyle(Color(red: 0.918, green: 0.763, blue: 0.763))
                    .multilineTextAlignment(.center)
                    .frame(width: 86.0, height: 89.0)
                    .position(x: 68.0, y: 154.5)
                Text("段落テキストは折り返される")
                    .font(.custom("Koz Go Pr6N", size: 24))
                    .foregroundStyle(Color(red: 0.918, green: 0.763, blue: 0.763))
                    .multilineTextAlignment(.center)
                    .frame(width: 158.0, height: 116.0)
                    .position(x: 239.0, y: 168.0)
            }
        }
        .frame(width: 320.0, height: 240.0)
    }
}

#Preview {
    MainView()
}

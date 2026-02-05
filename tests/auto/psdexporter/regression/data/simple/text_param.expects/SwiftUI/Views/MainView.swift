import SwiftUI

struct MainView: View {
    var body: some View {
        ZStack {
            Group {
                Rectangle
                    .fill(Color(red: 0.106, green: 0.525, blue: 0.541))
                    .frame(width: 320.0, height: 240.0)
                    .position(x: 160.0, y: 120.0)
                Text("")
                    .font(.custom("SourceHanSans-Medium", size: 50))
                    .foregroundStyle(Color(red: 0.000, green: 0.000, blue: 0.000))
                    .multilineTextAlignment(.center)
                    .frame(width: 0.0, height: 54.0)
                    .position(x: 145.0, y: 47.0)
                Rectangle
                    .fill(Color(red: 0.659, green: 0.965, blue: 0.929))
                    .frame(width: 240.0, height: 97.0)
                    .position(x: 150.0, y: 69.0)
                Rectangle
                    .fill(Color(red: 0.498, green: 0.392, blue: 0.965))
                    .frame(width: 240.0, height: 50.0)
                    .position(x: 150.0, y: 45.0)
                Text("shooting")
                    .font(.custom("SourceHanSans-Medium", size: 50))
                    .foregroundStyle(Color(red: 0.000, green: 0.000, blue: 0.000))
                    .multilineTextAlignment(.center)
                    .frame(width: 239.0, height: 54.0)
                    .position(x: 149.5, y: 47.0)
            }
        }
        .frame(width: 320.0, height: 240.0)
    }
}

#Preview {
    MainView()
}

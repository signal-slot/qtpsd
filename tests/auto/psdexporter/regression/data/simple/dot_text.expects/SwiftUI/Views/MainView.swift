import SwiftUI

struct MainView: View {
    var body: some View {
        ZStack {
            Group {
                Rectangle
                    .fill(Color(red: 0.969, green: 0.906, blue: 0.565))
                    .frame(width: 320.0, height: 240.0)
                    .position(x: 160.0, y: 120.0)
                Text(".")
                    .font(.custom("Source Han Sans", size: 30))
                    .foregroundStyle(Color(red: 0.000, green: 0.000, blue: 0.000))
                    .multilineTextAlignment(.center)
                    .frame(width: 9.0, height: 33.0)
                    .position(x: 129.5, y: 95.5)
            }
        }
        .frame(width: 320.0, height: 240.0)
    }
}

#Preview {
    MainView()
}

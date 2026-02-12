import SwiftUI

struct MainView: View {
    var body: some View {
        ZStack {
            Group {
                Rectangle
                    .fill(Color(red: 0.004, green: 0.141, blue: 0.004))
                    .frame(width: 640.0, height: 240.0)
                    .position(x: 320.0, y: 120.0)
                Text("12pt")
                    .font(.custom("Source Han Sans", size: 50))
                    .foregroundStyle(Color(red: 1.000, green: 1.000, blue: 1.000))
                    .multilineTextAlignment(.center)
                    .frame(width: 120.0, height: 56.0)
                    .position(x: 90.0, y: 57.0)
                Text("150px")
                    .font(.custom("Source Han Sans", size: 150))
                    .foregroundStyle(Color(red: 1.000, green: 1.000, blue: 1.000))
                    .multilineTextAlignment(.center)
                    .frame(width: 478.0, height: 168.0)
                    .position(x: 389.0, y: 140.0)
            }
        }
        .frame(width: 640.0, height: 240.0)
    }
}

#Preview {
    MainView()
}

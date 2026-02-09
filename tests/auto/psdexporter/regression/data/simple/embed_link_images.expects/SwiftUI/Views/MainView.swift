import SwiftUI

struct MainView: View {
    var body: some View {
        ZStack {
            Group {
                Rectangle
                    .fill(Color(red: 0.976, green: 0.812, blue: 0.957))
                    .frame(width: 320.0, height: 240.0)
                    .position(x: 160.0, y: 120.0)
                Image("qtquick")
                    .resizable()
                    .aspectRatio(contentMode: .fit)
                    .frame(width: 64.0, height: 46.0)
                    .position(x: 72.0, y: 103.0)
                Image("qtquick")
                    .resizable()
                    .aspectRatio(contentMode: .fit)
                    .frame(width: 36.0, height: 48.0)
                    .position(x: 76.0, y: 40.0)
                Image("7c7c45584dc3695d")
                    .resizable()
                    .aspectRatio(contentMode: .fit)
                    .frame(width: 64.0, height: 46.0)
                    .position(x: 72.0, y: 173.0)
                Image("slint")
                    .resizable()
                    .aspectRatio(contentMode: .fit)
                    .frame(width: 36.0, height: 48.0)
                    .position(x: 168.0, y: 103.0)
                Image("55f66e81a5e64db8")
                    .resizable()
                    .aspectRatio(contentMode: .fit)
                    .frame(width: 36.0, height: 48.0)
                    .position(x: 168.0, y: 173.0)
                Image("flutter")
                    .resizable()
                    .aspectRatio(contentMode: .fit)
                    .frame(width: 32.0, height: 40.0)
                    .position(x: 256.0, y: 102.0)
                Image("b6566c7e55c3283e")
                    .resizable()
                    .aspectRatio(contentMode: .fit)
                    .frame(width: 32.0, height: 40.0)
                    .position(x: 256.0, y: 172.0)
            }
        }
        .frame(width: 320.0, height: 240.0)
    }
}

#Preview {
    MainView()
}

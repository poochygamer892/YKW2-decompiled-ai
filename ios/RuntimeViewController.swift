import UIKit

/// Native host for the platform-neutral RuntimeScene.
final class RuntimeViewController: UIViewController {
    private let cacheURL: URL
    private let statusLabel = UILabel()
    private let selectedLabel = UILabel()
    private let commandLabel = UILabel()

    init(cacheURL: URL) {
        self.cacheURL = cacheURL
        super.init(nibName: nil, bundle: nil)
    }

    required init?(coder: NSCoder) {
        fatalError("RuntimeViewController does not use storyboards")
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        view.backgroundColor = .systemBackground
        title = "Yo-kai Watch 2"
        statusLabel.numberOfLines = 0
        statusLabel.textAlignment = .center
        selectedLabel.font = .preferredFont(forTextStyle: .title2)
        selectedLabel.textAlignment = .center
        commandLabel.numberOfLines = 0
        commandLabel.textAlignment = .center

        let previous = UIButton(type: .system)
        previous.setTitle("Anterior", for: .normal)
        previous.addTarget(self, action: #selector(movePrevious), for: .touchUpInside)
        let next = UIButton(type: .system)
        next.setTitle("Siguiente", for: .normal)
        next.addTarget(self, action: #selector(moveNext), for: .touchUpInside)
        let confirm = UIButton(type: .system)
        confirm.setTitle("Confirmar", for: .normal)
        confirm.addTarget(self, action: #selector(confirmSelection), for: .touchUpInside)

        let stack = UIStackView(arrangedSubviews: [statusLabel, selectedLabel, previous, next, confirm, commandLabel])
        stack.axis = .vertical
        stack.spacing = 20
        stack.translatesAutoresizingMaskIntoConstraints = false
        view.addSubview(stack)
        NSLayoutConstraint.activate([
            stack.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor, constant: 24),
            stack.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor, constant: -24),
            stack.centerYAnchor.constraint(equalTo: view.safeAreaLayoutGuide.centerYAnchor),
        ])
        render(YKW2ImportBridge.startRuntime(at: cacheURL))
    }

    @objc private func movePrevious() {
        render(YKW2ImportBridge.handleRuntimeInput(1))
    }

    @objc private func moveNext() {
        render(YKW2ImportBridge.handleRuntimeInput(0))
    }

    @objc private func confirmSelection() {
        render(YKW2ImportBridge.handleRuntimeInput(2))
    }

    private func render(_ response: String) {
        let lines = response.split(separator: "\n", omittingEmptySubsequences: true)
        var selected = 0
        var status = "runtime.title_menu"
        var command = "none"
        var items: [[String]] = []
        for line in lines {
            let parts = line.split(separator: "=", maxSplits: 1).map(String.init)
            guard parts.count == 2 else { continue }
            switch parts[0] {
            case "selected": selected = Int(parts[1]) ?? 0
            case "status": status = parts[1]
            case "command": command = parts[1]
            case "item": items.append(parts[1].split(separator: "|").map(String.init))
            default: break
            }
        }
        statusLabel.text = statusLabel(for: status)
        if let item = items.indices.contains(selected) ? items[selected] : nil,
           item.count > 1 {
            selectedLabel.text = label(for: item[1])
        } else {
            selectedLabel.text = statusLabel(for: status)
        }
        commandLabel.text = command == "start_new_game"
            ? "La escena de juego se conectará en la siguiente fase."
            : command == "open_settings" ? "Las opciones se conectarán en la siguiente fase." : ""
    }

    private func label(for key: String) -> String {
        switch key {
        case "menu.new_game": return "Nueva partida"
        case "menu.continue": return "Continuar"
        case "menu.settings": return "Opciones"
        default: return key
        }
    }

    private func statusLabel(for key: String) -> String {
        switch key {
        case "runtime.title_menu": return "Contenido inicial validado"
        case "runtime.content_missing":
            return "Falta el archivo inicial yw2_a.fa o su índice ARC0 no es válido."
        case "import.required": return "Selecciona una copia autorizada para importar."
        case "import.incomplete": return "La importación está incompleta; vuelve a seleccionar el dump."
        default: return key
        }
    }
}

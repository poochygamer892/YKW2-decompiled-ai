import UIKit

/// A lightweight scene renderer shared by the portable runtime on iOS.
final class RuntimeSceneCanvasView: UIView {
    var backgroundImage: UIImage?
    private var screen = "title_menu"
    private var playerX: Float = 0
    private var playerY: Float = 0
    private var speaker = ""
    private var dialogueText = ""
    private var enemyID = ""
    private var playerHP = 0
    private var playerMaxHP = 1
    private var enemyHP = 0
    private var enemyMaxHP = 1
    private var meshVertices: [SIMD3<Float>] = []
    private var meshIndices: [Int] = []
    private var sceneHeightPercent: CGFloat = 58
    private var dialogueTopPercent: CGFloat = 55

    func render(screen: String, playerX: Float, playerY: Float, speaker: String,
                dialogueText: String, enemyID: String, playerHP: Int,
                playerMaxHP: Int, enemyHP: Int, enemyMaxHP: Int,
                sceneHeightPercent: CGFloat, dialogueTopPercent: CGFloat,
                meshVertices: [SIMD3<Float>] = [], meshIndices: [Int] = []) {
        self.screen = screen
        self.playerX = playerX
        self.playerY = playerY
        self.speaker = speaker
        self.dialogueText = dialogueText
        self.enemyID = enemyID
        self.playerHP = playerHP
        self.playerMaxHP = max(1, playerMaxHP)
        self.enemyHP = enemyHP
        self.enemyMaxHP = max(1, enemyMaxHP)
        self.meshVertices = meshVertices
        self.meshIndices = meshIndices
        self.sceneHeightPercent = min(90, max(20, sceneHeightPercent))
        self.dialogueTopPercent = min(90, max(30, dialogueTopPercent))
        setNeedsDisplay()
    }

    override func draw(_ rect: CGRect) {
        UIColor(red: 0.03, green: 0.04, blue: 0.06, alpha: 1).setFill()
        UIRectFill(rect)
        if let image = backgroundImage {
            let target = CGRect(x: 0, y: 0, width: bounds.width,
                               height: bounds.height * sceneHeightPercent / 100)
            let scale = min(target.width / image.size.width, target.height / image.size.height)
            let imageSize = CGSize(width: image.size.width * scale, height: image.size.height * scale)
            let imageRect = CGRect(x: target.midX - imageSize.width / 2,
                                   y: target.midY - imageSize.height / 2,
                                   width: imageSize.width, height: imageSize.height)
            image.draw(in: imageRect)
        }
        switch screen {
        case "title_menu": drawMenu()
        case "world": drawWorld()
        case "dialogue": drawDialogue()
        case "battle": drawBattle()
        default: drawMenu()
        }
    }

    private func drawMenu() {
        UIColor(red: 0.03, green: 0.05, blue: 0.1, alpha: 0.92).setFill()
        UIBezierPath(roundedRect: CGRect(x: 12, y: 12, width: bounds.width - 24,
                                         height: bounds.height * 0.3), cornerRadius: 16).fill()
        drawText("Yo-kai Watch 2", at: CGPoint(x: 28, y: 34), color: .white, size: 28)
        drawText("Cliente móvil · recursos YW2", at: CGPoint(x: 28, y: 72),
                 color: UIColor(red: 0.52, green: 0.86, blue: 0.94, alpha: 1), size: 14)
    }

    private func drawWorld() {
        let top = bounds.height * 0.08
        let bottom = bounds.height * 0.72
        UIColor(red: 0.4, green: 0.75, blue: 0.8, alpha: 0.3).setStroke()
        if meshVertices.isEmpty || meshIndices.count < 3 {
            let cell = max(24, bounds.width / 9)
            for x in stride(from: CGFloat(0), through: bounds.width, by: cell) {
                let path = UIBezierPath()
                path.move(to: CGPoint(x: x, y: top))
                path.addLine(to: CGPoint(x: x, y: bottom))
                path.stroke()
            }
            for y in stride(from: top, through: bottom, by: cell) {
                let path = UIBezierPath()
                path.move(to: CGPoint(x: 0, y: y))
                path.addLine(to: CGPoint(x: bounds.width, y: y))
                path.stroke()
            }
        } else {
            let minX = meshVertices.map { $0.x }.min() ?? 0
            let maxX = meshVertices.map { $0.x }.max() ?? 1
            let minZ = meshVertices.map { $0.z }.min() ?? 0
            let maxZ = meshVertices.map { $0.z }.max() ?? 1
            let spanX = max(0.001, maxX - minX)
            let spanZ = max(0.001, maxZ - minZ)
            func point(_ index: Int) -> CGPoint {
                let vertex = meshVertices[index]
                return CGPoint(x: CGFloat((vertex.x - minX) / spanX) * bounds.width,
                               y: bottom - CGFloat((vertex.z - minZ) / spanZ) * (bottom - top))
            }
            UIColor(red: 0.35, green: 0.88, blue: 1, alpha: 0.6).setStroke()
            for index in stride(from: 0, to: meshIndices.count - 2, by: 3) {
                let a = meshIndices[index]
                let b = meshIndices[index + 1]
                let c = meshIndices[index + 2]
                guard meshVertices.indices.contains(a), meshVertices.indices.contains(b),
                      meshVertices.indices.contains(c) else { continue }
                let path = UIBezierPath()
                path.move(to: point(a)); path.addLine(to: point(b))
                path.addLine(to: point(c)); path.close()
                path.stroke()
            }
        }
        UIColor(red: 0.26, green: 0.85, blue: 0.75, alpha: 1).setFill()
        let point = CGPoint(x: bounds.midX + CGFloat(playerX) * 8,
                            y: (top + bottom) / 2 + CGFloat(playerY) * 8)
        UIBezierPath(ovalIn: CGRect(x: point.x - 14, y: point.y - 14, width: 28, height: 28)).fill()
        drawText("Exploración", at: CGPoint(x: 16, y: bottom + 8), color: .white, size: 14)
    }

    private func drawDialogue() {
        let panelTop = bounds.height * dialogueTopPercent / 100
        UIColor(red: 0.03, green: 0.05, blue: 0.1, alpha: 0.92).setFill()
        UIBezierPath(roundedRect: CGRect(x: 12, y: panelTop, width: bounds.width - 24,
                                         height: bounds.height - panelTop - 10), cornerRadius: 14).fill()
        drawText("Inspección de apertura",
                 at: CGPoint(x: 28, y: panelTop + 14), color: UIColor(
            red: 0.42, green: 0.86, blue: 1, alpha: 1), size: 17)
        (dialogueText as NSString).draw(in: CGRect(x: 28, y: panelTop + 47,
            width: bounds.width - 56, height: bounds.height - panelTop - 55), withAttributes: [
            .font: UIFont.systemFont(ofSize: 16), .foregroundColor: UIColor.white])
    }

    private func drawBattle() {
        UIColor(red: 0.06, green: 0.1, blue: 0.18, alpha: 0.95).setFill()
        UIBezierPath(roundedRect: bounds.insetBy(dx: 12, dy: 12), cornerRadius: 16).fill()
        drawText("Catálogo de combate YW2", at: CGPoint(x: 18, y: 28), color: .white, size: 17)
        drawText("Comandos leídos del dump. No hay encuentro ni reglas ejecutándose.",
                 at: CGPoint(x: 18, y: 64), color: .white, size: 14)
    }

    private func drawText(_ text: String, at point: CGPoint, color: UIColor, size: CGFloat) {
        text.draw(at: point, withAttributes: [
            .font: UIFont.systemFont(ofSize: size),
            .foregroundColor: color,
        ])
    }
}

/// Native host for the platform-neutral RuntimeScene.
final class RuntimeViewController: UIViewController {
    private let cacheURL: URL
    private let statusLabel = UILabel()
    private let selectedLabel = UILabel()
    private let commandLabel = UILabel()
    private let sceneView = RuntimeSceneCanvasView()
    private let sceneContainer = UIView()
    private let menuPanel = UIStackView()
    private var menuButtons: [UIButton] = []
    private var confirmButton: UIButton!
    private var battleControls: UIStackView!
    private var backButton: UIButton!
    private var displayLink: CADisplayLink?

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
        selectedLabel.numberOfLines = 0
        commandLabel.numberOfLines = 0
        commandLabel.textAlignment = .center

        let attack = button("Comando 1", #selector(attack))
        let skill = button("Comando 2", #selector(skill))
        backButton = button("Volver al menú", #selector(moveBack))
        confirmButton = button("Siguiente paso", #selector(confirmSelection))
        battleControls = UIStackView(arrangedSubviews: [attack, skill])
        battleControls.axis = .horizontal
        battleControls.distribution = .fillEqually

        sceneContainer.addSubview(sceneView)
        sceneView.translatesAutoresizingMaskIntoConstraints = false
        menuPanel.axis = .vertical
        menuPanel.spacing = 5
        menuPanel.backgroundColor = UIColor(red: 0.035, green: 0.08, blue: 0.12, alpha: 0.94)
        menuPanel.layer.cornerRadius = 16
        menuPanel.layoutMargins = UIEdgeInsets(top: 10, left: 10, bottom: 10, right: 10)
        menuPanel.isLayoutMarginsRelativeArrangement = true
        menuPanel.translatesAutoresizingMaskIntoConstraints = false
        for index in 0..<4 {
            let item = button("", #selector(selectMenuItem(_:)))
            item.tag = index
            item.titleLabel?.font = .systemFont(ofSize: 15, weight: .semibold)
            item.layer.cornerRadius = 11
            item.heightAnchor.constraint(greaterThanOrEqualToConstant: 45).isActive = true
            menuPanel.addArrangedSubview(item)
            menuButtons.append(item)
        }
        sceneContainer.addSubview(menuPanel)
        NSLayoutConstraint.activate([
            sceneView.leadingAnchor.constraint(equalTo: sceneContainer.leadingAnchor),
            sceneView.trailingAnchor.constraint(equalTo: sceneContainer.trailingAnchor),
            sceneView.topAnchor.constraint(equalTo: sceneContainer.topAnchor),
            sceneView.bottomAnchor.constraint(equalTo: sceneContainer.bottomAnchor),
            menuPanel.leadingAnchor.constraint(equalTo: sceneContainer.leadingAnchor, constant: 10),
            menuPanel.trailingAnchor.constraint(equalTo: sceneContainer.trailingAnchor, constant: -10),
            menuPanel.bottomAnchor.constraint(equalTo: sceneContainer.bottomAnchor, constant: -10),
        ])

        let stack = UIStackView(arrangedSubviews: [statusLabel, sceneContainer, selectedLabel,
            confirmButton, battleControls, backButton, commandLabel])
        stack.axis = .vertical
        stack.spacing = 12
        stack.translatesAutoresizingMaskIntoConstraints = false
        sceneContainer.heightAnchor.constraint(equalToConstant: 460).isActive = true
        let scroll = UIScrollView()
        scroll.translatesAutoresizingMaskIntoConstraints = false
        scroll.addSubview(stack)
        view.addSubview(scroll)
        NSLayoutConstraint.activate([
            scroll.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor),
            scroll.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor),
            scroll.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor),
            scroll.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor),
            stack.leadingAnchor.constraint(equalTo: scroll.contentLayoutGuide.leadingAnchor, constant: 24),
            stack.trailingAnchor.constraint(equalTo: scroll.contentLayoutGuide.trailingAnchor, constant: -24),
            stack.topAnchor.constraint(equalTo: scroll.contentLayoutGuide.topAnchor, constant: 12),
            stack.bottomAnchor.constraint(equalTo: scroll.contentLayoutGuide.bottomAnchor, constant: -12),
            stack.widthAnchor.constraint(equalTo: scroll.frameLayoutGuide.widthAnchor, constant: -48),
        ])
        render(YKW2ImportBridge.startRuntime(at: cacheURL))
        renderBackground()
    }

    override func viewDidAppear(_ animated: Bool) {
        super.viewDidAppear(animated)
        displayLink?.invalidate()
        displayLink = CADisplayLink(target: self, selector: #selector(advanceFrame(_:)))
        displayLink?.add(to: .main, forMode: .common)
    }

    override func viewWillDisappear(_ animated: Bool) {
        displayLink?.invalidate()
        displayLink = nil
        super.viewWillDisappear(animated)
    }

    private func button(_ title: String, _ action: Selector) -> UIButton {
        let result = UIButton(type: .system)
        result.setTitle(title, for: .normal)
        result.addTarget(self, action: action, for: .touchUpInside)
        return result
    }

    private func updateControls(for screen: String) {
        menuPanel.isHidden = screen != "title_menu"
        confirmButton.isHidden = screen != "dialogue"
        battleControls.isHidden = screen != "battle"
        backButton.isHidden = screen != "dialogue" && screen != "battle"
    }

    @objc private func selectMenuItem(_ sender: UIButton) {
        render(YKW2ImportBridge.selectRuntimeMenuItem(sender.tag))
    }

    @objc private func advanceFrame(_ displayLink: CADisplayLink) {
        render(YKW2ImportBridge.advanceRuntime(1.0 / 60.0))
    }

    @objc private func movePrevious() { render(YKW2ImportBridge.handleRuntimeInput(1)) }
    @objc private func moveNext() { render(YKW2ImportBridge.handleRuntimeInput(0)) }
    @objc private func confirmSelection() { render(YKW2ImportBridge.handleRuntimeInput(2)) }
    @objc private func moveUp() { render(YKW2ImportBridge.handleRuntimeInput(4)) }
    @objc private func moveDown() { render(YKW2ImportBridge.handleRuntimeInput(5)) }
    @objc private func moveLeft() { render(YKW2ImportBridge.handleRuntimeInput(6)) }
    @objc private func moveRight() { render(YKW2ImportBridge.handleRuntimeInput(7)) }
    @objc private func attack() { render(YKW2ImportBridge.handleRuntimeInput(8)) }
    @objc private func skill() { render(YKW2ImportBridge.handleRuntimeInput(9)) }
    @objc private func run() { render(YKW2ImportBridge.handleRuntimeInput(10)) }
    @objc private func moveBack() { render(YKW2ImportBridge.handleRuntimeInput(3)) }

    private func render(_ response: String) {
        let lines = response.split(separator: "\n", omittingEmptySubsequences: true)
        var selected = 0
        var status = "runtime.title_menu"
        var command = "none"
        var screen = "title_menu"
        var sceneHeightPercent: CGFloat = 58
        var dialogueTopPercent: CGFloat = 55
        var items: [[String]] = []
        var sceneResourceCount = 0
        var worldMap = "mapa pendiente"
        var playerX: Float = 0
        var playerY: Float = 0
        var speaker = ""
        var eventID = ""
        var eventSymbol = ""
        var eventInstructions = 0
        var eventArguments = 0
        var eventDecodedInstructions = 0
        var lineIndex = 0
        var lineCount = 0
        var operationToken = ""
        var operationStatus = "idle"
        var argumentIndex = 0
        var argumentCount = 0
        var enemyID = ""
        var playerHP = 0
        var playerMaxHP = 1
        var enemyHP = 0
        var enemyMaxHP = 1
        var battleDefinition = "sin definición"
        var battleCommandCount = 0
        var battleLastCommand = ""
        var meshVertices: [SIMD3<Float>] = []
        var meshIndices: [Int] = []
        for line in lines {
            let parts = line.split(separator: "=", maxSplits: 1).map(String.init)
            guard parts.count == 2 else { continue }
            switch parts[0] {
            case "screen": screen = parts[1]
            case "selected": selected = Int(parts[1]) ?? 0
            case "status": status = parts[1]
            case "presentation_scene_height_percent": sceneHeightPercent = CGFloat(Double(parts[1]) ?? 58)
            case "presentation_dialogue_top_percent": dialogueTopPercent = CGFloat(Double(parts[1]) ?? 55)
            case "command": command = parts[1]
            case "item": items.append(parts[1].split(separator: "|").map(String.init))
            case "scene_resource": sceneResourceCount += 1
            case "world_map": worldMap = parts[1]
            case "player_x": playerX = Float(parts[1]) ?? 0
            case "player_y": playerY = Float(parts[1]) ?? 0
            case "speaker": speaker = parts[1]
            case "event_id": eventID = parts[1]
            case "event_symbol": eventSymbol = parts[1]
            case "event_instructions": eventInstructions = Int(parts[1]) ?? 0
            case "event_arguments": eventArguments = Int(parts[1]) ?? 0
            case "event_decoded_instructions": eventDecodedInstructions = Int(parts[1]) ?? 0
            case "line_index": lineIndex = Int(parts[1]) ?? 0
            case "line_count": lineCount = Int(parts[1]) ?? 0
            case "event_exec_operation": operationToken = parts[1]
            case "event_exec_operation_status": operationStatus = parts[1]
            case "event_exec_argument_index": argumentIndex = Int(parts[1]) ?? 0
            case "event_exec_argument_count": argumentCount = Int(parts[1]) ?? 0
            case "enemy_id": enemyID = parts[1]
            case "player_hp": playerHP = Int(parts[1]) ?? 0
            case "player_max_hp": playerMaxHP = Int(parts[1]) ?? 1
            case "enemy_hp": enemyHP = Int(parts[1]) ?? 0
            case "enemy_max_hp": enemyMaxHP = Int(parts[1]) ?? 1
            case "battle_definition_id": battleDefinition = parts[1]
            case "battle_configured_command_count": battleCommandCount = Int(parts[1]) ?? 0
            case "battle_last_command_id": battleLastCommand = parts[1]
            case "scene_mesh_vertex":
                let values = parts[1].split(separator: "|").compactMap { Float(String($0)) }
                if values.count == 3 {
                    meshVertices.append(SIMD3<Float>(values[0], values[1], values[2]))
                }
            case "scene_mesh_index":
                if let index = Int(parts[1].split(separator: "|").last ?? "") { meshIndices.append(index) }
            default: break
            }
        }
        statusLabel.text = statusText(for: status, screen: screen)
        updateControls(for: screen)
        updateMenu(items: items, selected: selected)
        selectedLabel.text = screen == "dialogue" ?
            "Avance estructural del evento; todavía no reproduce la escena" :
            screen == "battle" ? "Selecciona un ID; no se ejecuta un combate" : ""
        let dialogue = eventDescription(eventID: eventID,
                                        instructions: eventInstructions,
                                        lineIndex: lineIndex, lineCount: lineCount,
                                        operationStatus: operationStatus)
        sceneView.render(screen: screen, playerX: playerX, playerY: playerY,
                         speaker: screen == "dialogue" ? "" : speaker,
                         dialogueText: dialogue, enemyID: enemyID, playerHP: playerHP,
                         playerMaxHP: playerMaxHP, enemyHP: enemyHP, enemyMaxHP: enemyMaxHP,
                         sceneHeightPercent: sceneHeightPercent,
                         dialogueTopPercent: dialogueTopPercent,
                         meshVertices: meshVertices, meshIndices: meshIndices)
        commandLabel.text = commandText(command: command, screen: screen, speaker: speaker,
                                        dialogue: dialogue, worldMap: worldMap, playerX: playerX,
                                        playerY: playerY, sceneResourceCount: sceneResourceCount,
                                        enemyID: enemyID, enemyHP: enemyHP, enemyMaxHP: enemyMaxHP)
        if screen == "battle" {
            let selectedCommand = battleLastCommand.isEmpty
                ? "Ningún ID seleccionado"
                : "ID seleccionado: \(battleLastCommand)"
            commandLabel.text = "\(battleDefinition) · \(battleCommandCount) IDs indexados\n\(selectedCommand)"
        }
    }

    private func updateMenu(items: [[String]], selected: Int) {
        for (index, button) in menuButtons.enumerated() {
            guard items.indices.contains(index), items[index].count >= 3 else {
                button.isHidden = true
                continue
            }
            let item = items[index]
            let enabled = item[2] == "1"
            button.isHidden = false
            button.isEnabled = enabled
            button.setTitle(label(for: item[1]), for: .normal)
            button.setTitleColor(enabled ? .white : UIColor(white: 0.65, alpha: 1), for: .normal)
            button.backgroundColor = index == selected && enabled
                ? UIColor(red: 0.09, green: 0.44, blue: 0.56, alpha: 1)
                : UIColor(red: 0.09, green: 0.16, blue: 0.21, alpha: 1)
            button.layer.borderWidth = 1
            button.layer.borderColor = enabled
                ? UIColor(red: 0.26, green: 0.67, blue: 0.78, alpha: 1).cgColor
                : UIColor(red: 0.23, green: 0.3, blue: 0.34, alpha: 1).cgColor
        }
    }

    private func commandText(command: String, screen: String, speaker: String, dialogue: String,
                             worldMap: String, playerX: Float, playerY: Float,
                             sceneResourceCount: Int, enemyID: String, enemyHP: Int,
                             enemyMaxHP: Int) -> String {
        if screen == "dialogue" { return "La semántica de esta escena sigue pendiente." }
        if screen == "battle" { return "Catálogo de datos YW2; no hay encuentro en ejecución." }
        switch command {
        case "request_authorized_import": return "Selecciona una copia autorizada desde el menú de importación."
        default: break
        }
        if screen == "title_menu" {
            return "Nueva partida y Continuar se activarán cuando historia, mapa y guardado funcionen con datos YW2."
        }
        return ""
    }

    private func renderBackground() {
        let width = Int(YKW2ImportBridge.runtimeBackgroundWidth())
        let height = Int(YKW2ImportBridge.runtimeBackgroundHeight())
        let data = YKW2ImportBridge.runtimeBackgroundRGBA()
        guard width > 0, height > 0, data.count == width * height * 4,
              let provider = CGDataProvider(data: data as CFData),
              let image = CGImage(width: width, height: height, bitsPerComponent: 8,
                                  bitsPerPixel: 32, bytesPerRow: width * 4,
                                  space: CGColorSpaceCreateDeviceRGB(),
                                  bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.last.rawValue |
                                      CGBitmapInfo.byteOrder32Big.rawValue), provider: provider,
                                  decode: nil, shouldInterpolate: false, intent: .defaultIntent) else {
            return
        }
        sceneView.backgroundImage = UIImage(cgImage: image)
        sceneView.setNeedsDisplay()
    }

    private func eventDescription(eventID: String, instructions: Int,
                                  lineIndex: Int, lineCount: Int,
                                  operationStatus: String) -> String {
        if eventID.isEmpty { return "No se encontró un evento de apertura compatible en el dump." }
        let total = lineCount > 0 ? lineCount : instructions
        let step = total > 0 ? "Paso \(lineIndex + 1)/\(total)" : "Programa sin pasos"
        let operation: String
        switch operationStatus {
        case "unknown_operation": operation = "Paso estructural; operación aún no traducida"
        case "invalid_arguments": operation = "Paso detenido: argumentos no validados"
        case "completed": operation = "Evento completado"
        default: operation = "Paso de evento"
        }
        return "\(step)\n\(operation)"
    }

    private func label(for key: String) -> String {
        switch key {
        case "menu.new_game": return "Nueva partida"
        case "menu.continue": return "Continuar"
        case "menu.inspect_opening": return "Inspeccionar apertura YW2"
        case "menu.battle_catalog": return "Consultar catálogo de combate"
        default: return key
        }
    }

    private func statusText(for key: String, screen: String) -> String {
        if screen == "dialogue" { return "Evento de apertura · inspección" }
        if screen == "battle" { return "Datos de combate · catálogo" }
        switch key {
        case "runtime.title_menu": return "Yo-kai Watch 2 · prototipo móvil"
        case "runtime.content_missing": return "Falta el archivo inicial yw2_a.fa o su índice ARC0 no es válido."
        case "import.required": return "Selecciona una copia autorizada para importar."
        case "import.incomplete": return "La importación está incompleta; vuelve a seleccionar el dump."
        default: return key
        }
    }
}

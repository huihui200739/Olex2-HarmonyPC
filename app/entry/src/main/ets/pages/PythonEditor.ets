// Platform editing surface: selected text is never executed without Run.
@Component
export struct PythonEditor {
  @Link source: string;
  @Link dirty: boolean;
  @Prop busy: boolean = false;
  @Prop runtimeReady: boolean = false;
  @Prop message: string = '';
  @Prop output: string = '';
  @Prop error: string = '';
  onOpen: () => void = () => {};
  onSave: () => void = () => {};
  onNew: () => void = () => {};
  onClose: () => void = () => {};
  onRun: () => void = () => {};

  build() {
    Column({ space: 10 }) {
      Row({ space: 8 }) {
        Text('Python 脚本编辑器').fontSize(19).fontWeight(FontWeight.Medium).fontColor('#253D30').layoutWeight(1)
        Text(this.dirty ? '未保存修改' : '会话草稿').id('olex2-python-draft-state').fontSize(11).fontColor('#738078')
        Button('返回工作台').id('olex2-python-editor-close').height(32).fontSize(12)
          .enabled(!this.busy).onClick(() => this.onClose())
      }.width('100%')
      Row({ space: 8 }) {
        Button('新建').id('olex2-python-editor-new').height(32).fontSize(12).enabled(!this.busy).onClick(() => this.onNew())
        Button('打开脚本…').id('olex2-python-editor-open').height(32).fontSize(12).enabled(!this.busy).onClick(() => this.onOpen())
        Button('另存为…').id('olex2-python-editor-save').height(32).fontSize(12)
          .enabled(!this.busy && this.source.length > 0).onClick(() => this.onSave())
        Blank()
        Button('运行脚本').id('olex2-python-editor-run').height(32).fontSize(12).backgroundColor('#327550')
          .enabled(!this.busy && this.runtimeReady && this.source.trim().length > 0).onClick(() => this.onRun())
      }.width('100%')
      Text('仅运行可信脚本；与原应用共用 Python，非安全沙箱。打开文件不会自动运行；当前仅支持同步执行。')
        .fontSize(11).fontColor('#738078').width('100%').maxLines(2)
      TextArea({ text: this.source, placeholder: '在这里输入多行 Python，或打开 UTF-8 .py 文件…' })
        .id('olex2-python-editor-source').width('100%').layoutWeight(1).fontSize(14).fontFamily('monospace')
        .maxLength(65536).backgroundColor('#FFFFFF').enabled(!this.busy).defaultFocus(true)
        .onChange((value: string) => { if (value !== this.source) { this.source = value; this.dirty = true; } })
      Text(this.busy ? '等待系统文件选择…' : this.message).id('olex2-python-document-status')
        .fontSize(11).fontColor('#627567').width('100%').maxLines(2)
      Text('上次执行结果').fontSize(12).fontWeight(FontWeight.Medium).fontColor('#355443').width('100%')
      Scroll() {
        Text(this.output + (this.error.length > 0 ? '\n' + this.error : ''))
          .id('olex2-python-editor-output').width('100%').fontSize(12).fontFamily('monospace')
          .fontColor(this.error.length > 0 ? '#A24F3D' : '#355443').copyOption(CopyOptions.InApp)
      }.width('100%').height('20%').backgroundColor('#E7EEE9').padding(10).borderRadius(8)
    }.id('olex2-python-editor').padding(18).width('100%').height('100%').backgroundColor('#F4F7F3')
  }
}

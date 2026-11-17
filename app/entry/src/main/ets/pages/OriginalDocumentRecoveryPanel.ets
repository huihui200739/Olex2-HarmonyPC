// Presentation only: the parent owns private backups, confirmation and recovery.
// No model imports, lifecycle work, file access or implicit recovery/checkpoint.
@Component
export struct OriginalDocumentRecoveryPanel {
  @Prop state: string = 'unavailable';
  @Prop documentCount: number = 0;
  @Prop generation: number = 0;
  @Prop savedAt: string = '';
  @Prop detail: string = '';
  @Prop error: string = '';
  @Prop busy: boolean = false;
  @Prop canRestore: boolean = false;
  @Prop canDiscard: boolean = false;
  @Prop canCheckpoint: boolean = false;
  onRestore: () => void = () => {};
  onDiscard: () => void = () => {};
  onCheckpoint: () => void = () => {};

  private hasFailure(): boolean {
    return this.error.trim().length > 0 || this.state === 'error' || this.state === 'failed' ||
      this.state === 'restore-failed' || this.state === 'checkpoint-failed' || this.state === 'write-failed';
  }

  private hasPendingRecovery(): boolean {
    return this.canRestore || this.state === 'pending' || this.state === 'available' || this.state === 'recoverable';
  }

  private hasSuccessfulCheckpoint(): boolean {
    // Metadata alone is not proof of a successful backup or an original-file save.
    return this.state === 'saved' || this.state === 'checkpointed' || this.state === 'backed-up';
  }

  private stateLabel(): string {
    if (this.busy) return '正在处理会话，请稍候…';
    if (this.hasFailure()) return '恢复或备份失败，旧副本仍保留';
    if (this.hasPendingRecovery()) return '发现上次会话备份，等待你确认恢复';
    if (this.hasSuccessfulCheckpoint()) return '当前会话最近一次备份成功';
    if (this.state === 'restored') return '上次会话已恢复，尚不代表原文件已保存';
    if (this.state === 'empty') return '没有可恢复文档';
    if (this.state === 'unavailable') return '会话备份暂不可用';
    return '会话备份状态待确认';
  }

  private stateHint(): string {
    if (this.busy) return '处理完成后再操作；不会自动保存到原文件。';
    if (this.hasFailure()) return '请查看提示后重试；不会自动恢复或改写原文件。';
    if (this.hasPendingRecovery()) {
      return '仅在你点击“恢复上次会话”后恢复。“不恢复，开始新会话”还需再次确认。';
    }
    if (this.hasSuccessfulCheckpoint()) return '这里只记录最近成功的备份；之后的修改仍需再次备份。';
    if (this.state === 'restored') return '恢复的是会话内容。需要保留到原文件时，请另行点击“保存”。';
    if (this.state === 'empty') return '打开文档后，可点击“立即备份会话”。';
    return '操作由你确认，面板不会自动恢复或备份。';
  }

  private detailsLabel(): string {
    const count: number = Math.max(0, Math.floor(this.documentCount));
    const sequence: number = Math.max(0, Math.floor(this.generation));
    const time: string = this.savedAt.trim().length > 0 ? this.savedAt : '暂无成功备份时间';
    const rawState: string = this.state.trim().length > 0 ? this.state : '未提供';
    return '文档数：' + count + ' · 备份批次：' + sequence + '\n最近成功备份：' + time +
      '\n状态记录：' + rawState + (this.detail.trim().length > 0 ? '\n' + this.detail : '');
  }

  build() {
    Column({ space: 8 }) {
      Text('会话恢复与备份')
        .fontSize(14).fontWeight(FontWeight.Medium).fontColor('#253D30').width('100%')
        .wordBreak(WordBreak.BREAK_ALL)
      Text(this.stateLabel()).id('olex2-recovery-state')
        .fontSize(13).fontWeight(FontWeight.Medium).width('100%')
        .fontColor(this.hasFailure() ? '#945617' : '#253D30').wordBreak(WordBreak.BREAK_ALL)
      Text(this.stateHint())
        .fontSize(12).fontColor('#516A5A').width('100%').wordBreak(WordBreak.BREAK_ALL)
      Text(this.detailsLabel()).id('olex2-recovery-details')
        .fontSize(11).fontColor('#516A5A').width('100%').wordBreak(WordBreak.BREAK_ALL)
      if (this.error.trim().length > 0) {
        Text(this.error).id('olex2-recovery-error')
          .fontSize(12).fontColor('#945617').width('100%').wordBreak(WordBreak.BREAK_ALL)
      }
      // Vertical, intrinsic-height actions also wrap in a narrow host window.
      Column({ space: 6 }) {
        Button({ type: ButtonType.Normal }) {
          Text('恢复上次会话').fontSize(12).fontColor('#FFFFFF')
            .width('100%').textAlign(TextAlign.Center).wordBreak(WordBreak.BREAK_ALL)
        }
        .id('olex2-recovery-restore').width('100%').constraintSize({ minHeight: 40 })
        .padding({ left: 12, right: 12, top: 8, bottom: 8 }).backgroundColor('#327550').borderRadius(6)
        .accessibilityText('恢复上次会话，仅在点击后恢复应用私有副本')
        .enabled(this.canRestore && !this.busy)
        .onClick((): void => {
          if (this.canRestore && !this.busy) this.onRestore();
        })
        Button({ type: ButtonType.Normal }) {
          Text('不恢复，开始新会话').fontSize(12).fontColor('#253D30')
            .width('100%').textAlign(TextAlign.Center).wordBreak(WordBreak.BREAK_ALL)
        }
        .id('olex2-recovery-discard').width('100%').constraintSize({ minHeight: 40 })
        .padding({ left: 12, right: 12, top: 8, bottom: 8 }).backgroundColor('#FFFFFF').borderRadius(6)
        .accessibilityText('不恢复，开始新会话，接下来仍需再次确认')
        .enabled(this.canDiscard && !this.busy)
        .onClick((): void => {
          if (this.canDiscard && !this.busy) this.onDiscard();
        })
        Button({ type: ButtonType.Normal }) {
          Text('立即备份会话').fontSize(12).fontColor('#253D30')
            .width('100%').textAlign(TextAlign.Center).wordBreak(WordBreak.BREAK_ALL)
        }
        .id('olex2-recovery-checkpoint').width('100%').constraintSize({ minHeight: 40 })
        .padding({ left: 12, right: 12, top: 8, bottom: 8 }).backgroundColor('#FFFFFF').borderRadius(6)
        .accessibilityText('立即备份会话，只更新应用私有恢复副本，不保存到原文件')
        .enabled(this.canCheckpoint && !this.busy)
        .onClick((): void => {
          if (this.canCheckpoint && !this.busy) this.onCheckpoint();
        })
      }.width('100%').alignItems(HorizontalAlign.Start)
      Text('仅使用应用私有恢复副本，不会自动保存到原文件。重启后，保存位置及权限仍需通过真实“保存”操作核对。')
        .fontSize(11).fontColor('#516A5A').width('100%').wordBreak(WordBreak.BREAK_ALL)
    }
    .id('olex2-recovery-panel').padding(12).width('100%').alignItems(HorizontalAlign.Start)
    .backgroundColor('#EDF3EF').border({ width: 1, color: '#DCE6DE' }).borderRadius(8)
  }
}

// Hand-written surface declarations for tester.ts's untyped deps.
// (glob and colors ship their own types; temp does not.)

declare module "temp" {
    interface OpenFileInfo {
        path: string;
        fd: number;
    }
    export function open(
        affixes: string,
        callback: (err: Error | null, info: OpenFileInfo) => void
    ): void;
}

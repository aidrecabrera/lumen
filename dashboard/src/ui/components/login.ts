import { html, render } from "lit-html";

export interface Credentials {
  username: string;
  password: string;
}

// the password stays in memory only; nothing is written to storage.
export function askForCredentials(root: HTMLElement, title: string): Promise<Credentials> {
  return new Promise((resolve) => {
    const submit = (event: SubmitEvent): void => {
      event.preventDefault();

      if (!(event.currentTarget instanceof HTMLFormElement)) return;

      const data = new FormData(event.currentTarget);

      resolve({ username: String(data.get("username") ?? ""), password: String(data.get("password") ?? "") });
    };

    render(
      html`<form class="login" @submit=${submit}>
        <h1 class="app__title">${title}</h1>
        <label class="login__field">Username <input name="username" type="text" autocomplete="username" required /></label>
        <label class="login__field">Password <input name="password" type="password" autocomplete="current-password" required /></label>
        <button type="submit" data-variant="primary">Sign in</button>
      </form>`,
      root,
    );
  });
}
